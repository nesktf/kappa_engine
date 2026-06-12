#include "./scene.hpp"
#include "./context.hpp"

#include "../util/filesystem.hpp"
#include "../util/logger.hpp"

#include <imgui.h>

namespace kappa::render {

SceneData::SceneData(create_t, RenderContext& ctx, ComputeData&& compute, SceneLayouts&& layouts) :
    _ctx(&ctx), _compute(std::move(compute)), _meshes(), _layouts(std::move(layouts)) {}

namespace {

fn init_compute(RenderContext& ctx, SceneData::ComputeData& compute) -> void {
  auto& vk = ctx.get_vk();
  auto& delqueue = ctx.get_delqueue();
  auto& desc_alloc = ctx.get_desc_alloc();
  auto& target = ctx.get_target();

  VkShaderModule shader_gradient = VK_NULL_HANDLE, shader_sky = VK_NULL_HANDLE;
  const DeferFn shader_defer = [&]() {
    vk_destroy_shader(vk, shader_gradient);
    vk_destroy_shader(vk, shader_sky);
  };

  const auto src_gradient = load_entire_file(KA_RES_DIR "/shaders/gradient_color.comp.spv");
  shader_gradient = vk_create_shader(vk, {src_gradient.data(), src_gradient.size()}).value();

  const auto src_sky = load_entire_file(KA_RES_DIR "/shaders/sky.comp.spv");
  shader_sky = vk_create_shader(vk, {src_sky.data(), src_sky.size()}).value();

  VkDescLayoutBuilder desc_layout_builder;
  compute.image_desc_layout = desc_layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                                .build(vk, VK_SHADER_STAGE_COMPUTE_BIT)
                                .value();
  delqueue.enqueue(compute.image_desc_layout, vk.device());

  compute.image_desc = desc_alloc.allocate(compute.image_desc_layout).value();

  VkDescWriter writer(vk.device());
  writer.write_storage_image(0, target.color.view(), VK_IMAGE_LAYOUT_GENERAL);
  writer.update_set(compute.image_desc);

  VkPipelineLayoutBuilder layout_builder;
  compute.layout =
    layout_builder.add_layout(compute.image_desc_layout)
      .add_push_range(VK_SHADER_STAGE_COMPUTE_BIT, sizeof(SceneData::ComputeConstants), 0)
      .build(vk)
      .value();
  delqueue.enqueue(compute.layout, vk.device());

  compute.pipelines[0] = vk_create_compute_pipeline(vk, compute.layout, shader_gradient).value();
  delqueue.enqueue(compute.pipelines[0], vk.device());

  compute.pipelines[1] = vk_create_compute_pipeline(vk, compute.layout, shader_sky).value();
  delqueue.enqueue(compute.pipelines[1], vk.device());

  compute.data[0].data1 = ran::Vec4f32(1.f, 0.f, 0.f, 1.f);
  compute.data[0].data2 = ran::Vec4f32(0.f, 0.f, 1.f, 1.f);
  compute.data[1].data1 = ran::Vec4f32(.1f, .2f, .4f, .97f);
}

fn init_layouts(RenderContext& ctx, SceneData::SceneLayouts& layouts) -> void {
  auto& vk = ctx.get_vk();
  auto& delqueue = ctx.get_delqueue();

  VkDescLayoutBuilder builder;
  layouts.image_layout = builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                           .build(vk, VK_SHADER_STAGE_FRAGMENT_BIT)
                           .value();
  delqueue.enqueue(layouts.image_layout, vk.device());

  builder.clear();
  layouts.main_layout = builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                          .build(vk, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                          .value();
  delqueue.enqueue(layouts.main_layout, vk.device());
}

} // namespace

fn SceneData::initialize(Uninited<SceneData> scene, RenderContext& ctx) -> VkMsgExpect<void> {
  ComputeData compute;
  init_compute(ctx, compute);

  SceneLayouts layouts;
  init_layouts(ctx, layouts);

  KA_PNEW(scene) SceneData(create_t(), ctx, std::move(compute), std::move(layouts));
  return {};
}

namespace {

struct Vertex {
  ran::Vec3f32 pos;
  f32 uv_x;
  ran::Vec3f32 normal;
  f32 uv_y;
  ran::Vec4f32 color;
};

fn soa_to_aos(Span<const ran::Vec3f32> pos, Span<const ran::Vec2f32> uvs) -> UniqueArray<Vertex> {
  const usize count = pos.size();
  ka_assert(uvs.size() == count);
  auto out = make_array<Vertex>(uninitialized, count);
  for (usize i = 0; i < count; ++i) {
    out[i].color = ran::Vec4f32(1.f, 1.f, 1.f, 1.f); // make something up
    out[i].uv_x = uvs[i].x;
    out[i].uv_y = uvs[i].y;
    out[i].pos = pos[i];
  }
  return out;
}

fn copy_buffers(VkContext& vk, VkAllocBuff& vb, const void* vb_data, VkDeviceSize vb_size,
                VkAllocBuff& ib, const void* ib_data, VkDeviceSize ib_size) -> void {
  const VkBufferArgs staging_args{
    .size = vb_size + ib_size,
    .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    .mem_usage = KA_VK_MEM_USAGE_CPU_ONLY,
  };
  auto staging = VkAllocBuff::create(vk.allocator(), staging_args).value();
  const DeferFn staging_defer = [&]() {
    vk_destroy_buffer(vk.allocator(), staging);
  };

  void* data = staging.mapped_data();
  std::memcpy(data, vb_data, vb_size);
  std::memcpy(((u8*)data) + vb_size, ib_data, ib_size);

  vk_submit_immediate(vk, [&](VkCommandBuffer cmd) -> void {
    VkBufferCopy vertex_copy{};
    vertex_copy.dstOffset = 0;
    vertex_copy.srcOffset = 0;
    vertex_copy.size = vb_size;
    vkCmdCopyBuffer(cmd, staging.buffer(), vb.buffer(), 1, &vertex_copy);

    VkBufferCopy index_copy{};
    index_copy.dstOffset = 0;
    index_copy.srcOffset = vb_size;
    index_copy.size = ib_size;
    vkCmdCopyBuffer(cmd, staging.buffer(), ib.buffer(), 1, &index_copy);
  });
}

struct MeshConstants {
  ran::Mat4f32 world;
  ran::Mat4f32 proj;
  ran::Mat4f32 view;
  VkDeviceAddress vertex_buffer;
};

fn init_pipeline(RenderContext& ctx, VkDescriptorSetLayout image_layout)
  -> std::pair<VkPipeline, VkPipelineLayout> {
  auto& vk = ctx.get_vk();
  auto& delqueue = ctx.get_delqueue();
  auto& target = ctx.get_target();

  VkShaderModule frag = VK_NULL_HANDLE, vert = VK_NULL_HANDLE;
  const DeferFn shader_defer = [&]() {
    vk_destroy_shader(vk, frag);
    vk_destroy_shader(vk, vert);
  };
  VkPipelineLayout layout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;

  const auto mesh_vert_src = load_entire_file(KA_RES_DIR "/shaders/colored_mesh.vert.spv");
  vert = vk_create_shader(vk, {mesh_vert_src.data(), mesh_vert_src.size()}).value();

  const auto frag_src = load_entire_file(KA_RES_DIR "/shaders/colored_triangle.frag.spv");
  frag = vk_create_shader(vk, {frag_src.data(), frag_src.size()}).value();

  VkPipelineLayoutBuilder layout_builder;
  layout = layout_builder.add_push_range(VK_SHADER_STAGE_VERTEX_BIT, sizeof(MeshConstants), 0)
             .add_layout(image_layout)
             .build(vk)
             .value();
  delqueue.enqueue(layout, vk.device());

  VkGfxPipelineBuilder pipeline_builder;
  pipeline = pipeline_builder.set_layout(layout)
               .add_module(VK_SHADER_STAGE_VERTEX_BIT, vert)
               .add_module(VK_SHADER_STAGE_FRAGMENT_BIT, frag)
               .set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
               .set_poly_mode(VK_POLYGON_MODE_FILL)
               .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
               .set_color_format(target.color.format())
               .set_depth_format(target.depth.format())
               .enable_depth_test(KA_VK_DEPTH_WRITE_ENABLE, VK_COMPARE_OP_GREATER_OR_EQUAL)
               .disable_multisampling()
               .disable_blending()
               .build(vk)
               .value();
  delqueue.enqueue(pipeline, vk.device());
  return {pipeline, layout};
}

} // namespace

fn SceneData::add_mesh(const MeshData& mesh, std::string_view name) -> Mesh {
  log_debug(" Adding mesh: {}", name);
  auto& vk = _ctx->get_vk();

  // Vertex buffer
  const auto vertices = soa_to_aos(mesh.positions, mesh.uvs); // commit a crime
  const auto vb_size = vertices.size() * sizeof(vertices[0]);
  const VkBufferArgs vb_args{
    .size = vb_size,
    .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
             VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    .mem_usage = KA_VK_MEM_USAGE_GPU_ONLY,
  };
  auto vb = VkAllocBuff::create(vk.allocator(), vb_args).value();
  DeferFn vb_err = [&]() {
    vk_destroy_buffer(vk.allocator(), vb);
  };

  // Index buffer
  const auto ib_size = mesh.indices.size_bytes();
  const VkBufferArgs ib_args{
    .size = ib_size,
    .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    .mem_usage = KA_VK_MEM_USAGE_GPU_ONLY,
  };
  auto ib = VkAllocBuff::create(vk.allocator(), ib_args).value();
  DeferFn ib_err = [&]() {
    vk_destroy_buffer(vk.allocator(), ib);
  };

  const auto [pipeline, layout] = init_pipeline(*_ctx, _layouts.image_layout);
  BuffStr<256> mesh_name;
  mesh_name.copy_from(name.data(), name.size());

  copy_buffers(vk, vb, vertices.data(), vb_size, ib, mesh.indices.data(), ib_size);

  ib_err.disengage();
  vb_err.disengage();

  return _meshes.emplace(pipeline, layout, ran::Mat4f32::identity(), std::move(vb), std::move(ib),
                         0, (u32)mesh.indices.size(), mesh_name);
}

fn SceneData::clear() -> void {
  _meshes.for_each([&](MeshAsset& mesh) {
    vk_destroy_pipeline_layout(_ctx->get_vk(), mesh.layout);
    vk_destroy_pipeline(_ctx->get_vk(), mesh.pipeline);
    vk_destroy_buffer(_ctx->get_vk().allocator(), mesh.vertex_buffer);
    vk_destroy_buffer(_ctx->get_vk().allocator(), mesh.index_buffer);
  });
  _meshes.clear();
}

namespace {

fn draw_compute(const SceneData::ComputeData& compute, VkExtent2D target_extent,
                VkCommandBuffer cmd) -> void {
  const auto& data = compute.data[compute.effect_idx];
  const auto pipeline = compute.pipelines[compute.effect_idx];
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compute.layout, 0, 1,
                          &compute.image_desc, 0, nullptr);
  vkCmdPushConstants(cmd, compute.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(data), &data);
  vkCmdDispatch(cmd, std::ceil(target_extent.width / 16.f), std::ceil(target_extent.height / 16.f),
                1);
}

} // namespace

fn SceneData::render_geometry(VkImageLayout& target_layout, VkCommandBuffer cmd) -> void {
  auto& vk = _ctx->get_vk();
  auto& target = _ctx->get_target();
  auto& frame = _ctx->get_frame();

  // Draw using compute pipeline (we use a general layout)
  target_layout =
    vkcmd_transition_image(cmd, target.color.image(), target_layout, VK_IMAGE_LAYOUT_GENERAL);
  draw_compute(_compute, target.extent, cmd);

  // Draw using graphics pipelines (we use a color attachment layout)
  target_layout = vkcmd_transition_image(cmd, target.color.image(), target_layout,
                                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  const auto color_attachment =
    vkmk_attach_info(target.color.view(), nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  const auto depth_attachment =
    vkmk_depth_attach_info(target.depth.view(), VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  const auto render_info = vkmk_render_info(target.extent, &color_attachment, &depth_attachment);

  // Dynamic state
  VkViewport viewport{};
  viewport.x = 0;
  viewport.y = 0;
  viewport.width = target.extent.width;
  viewport.height = target.extent.height;
  viewport.minDepth = 0.f;
  viewport.maxDepth = 1.f;
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent = target.extent;
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  vkCmdBeginRendering(cmd, &render_info);
  _meshes.for_each([&](MeshAsset& model_mesh) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, model_mesh.pipeline);
    auto image_set = frame.desc_alloc.allocate(_layouts.image_layout).value();
    {
      VkDescWriter writer(vk.device());
      writer.write_combined_image(0, _ctx->get_image(0).view(),
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  _ctx->get_sampler(RenderContext::SAMPLER_NEAREST));
      writer.update_set(image_set);
    }
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, model_mesh.layout, 0, 1,
                            &image_set, 0, nullptr);

    MeshConstants push_constants;
    push_constants.world = model_mesh.transform;
    push_constants.view = ran::Mat4f32::identity();
    //  ran::translate(ran::Mat4f32::identity(), ran::Vec3f32(0.f, 0.f, -5.f));
    push_constants.proj = ran::Mat4f32::identity();
    //  ran::perspective(
    //  ran::rad(70.f), (f32)draw_extent.width / (f32)draw_extent.height, 10000.f, .1f);
    // push_constants.proj.y2 *= -1;
    push_constants.vertex_buffer = model_mesh.vertex_buffer.addr(vk.device());
    vkCmdPushConstants(cmd, model_mesh.layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(push_constants), &push_constants);
    vkCmdBindIndexBuffer(cmd, model_mesh.index_buffer.buffer(), model_mesh.index_start,
                         VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, model_mesh.index_count, 1, 0, 0, 0);
  });

  vkCmdEndRendering(cmd);
}

fn SceneData::render_imgui(const VkFrameContext& frame) -> void {
  KA_UNUSED(frame);
  static constexpr const char* compute_names[2] = {"gradient_color", "sky"};
  ImGui::ShowDemoWindow();

  if (ImGui::Begin("background")) {
    ImGui::Text("Selected effect: %s", compute_names[_compute.effect_idx]);
    ImGui::SliderInt("Effect Index", &_compute.effect_idx, 0, 1);
    ImGui::InputFloat4("data1", _compute.data[_compute.effect_idx].data1.data());
    ImGui::InputFloat4("data2", _compute.data[_compute.effect_idx].data2.data());
    ImGui::InputFloat4("data3", _compute.data[_compute.effect_idx].data3.data());
    ImGui::InputFloat4("data4", _compute.data[_compute.effect_idx].data4.data());
  }
  ImGui::End();

#if 0
  if (ImGui::Begin("render options")) {
    ImGui::InputFloat("Render scale", &self.target.scale, .3f, 1.f);
    ImGui::Text("Render Extent: (%u x %u)", self.target.extent.width, self.target.extent.height);
    ImGui::Text("Swapchain Extent: (%u x %u)", swapchain_extent.width, swapchain_extent.height);
  }
  ImGui::End();
#endif

  if (ImGui::Begin("meshes")) {
    _meshes.for_each([&](MeshAsset& mesh) { ImGui::Text("Mesh: %s", mesh.name.c_str()); });
  }
  ImGui::End();
}

} // namespace kappa::render
