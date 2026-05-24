#include "./context.hpp"

#include "../util/filesystem.hpp"
#include "../util/logger.hpp"

#define KA_APP_NAME    "Kappa"
#define KA_APP_VERSION VK_MAKE_VERSION(KA_VER_MAJ, KA_VER_MIN, KA_VER_REV)

namespace kappa::render {

namespace {

fn make_vk_defer(VkContext& vk) {
  return [&]() {
    vk_destroy_context(vk);
  };
}

fn make_imgui_defer(GLFWContext::ImGuiHandler& imgui) {
  return [&]() {
    render::vk_shutdown_imgui();
    imgui.destroy();
    ImGui::DestroyContext();
  };
}

fn make_delqueue_defer(VkContext& vk, VkDelQueue& delqueue) {
  return [&]() {
    vkDeviceWaitIdle(vk.device());
    delqueue.flush();
  };
}

fn init_compute(VkContext& vk, VkDelQueue& delqueue, VkDescAlloc& desc_alloc, VkAllocImage& target,
                RenderContext::ComputeRenderData& compute) -> void {
  VkShaderModule shader_gradient = VK_NULL_HANDLE, shader_sky = VK_NULL_HANDLE;
  const DeferFn shader_defer = [&]() {
    render::vk_destroy_shader(vk, shader_gradient);
    render::vk_destroy_shader(vk, shader_sky);
  };

  const auto src_gradient = load_entire_file(KA_RES_DIR "/shaders/gradient_color.comp.spv");
  shader_gradient =
    render::vk_create_shader(vk, {src_gradient.data(), src_gradient.size()}).value();

  const auto src_sky = load_entire_file(KA_RES_DIR "/shaders/sky.comp.spv");
  shader_sky = render::vk_create_shader(vk, {src_sky.data(), src_sky.size()}).value();

  render::VkDescLayoutBuilder desc_layout_builder;
  compute.image_desc_layout = desc_layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                                .build(vk, VK_SHADER_STAGE_COMPUTE_BIT)
                                .value();
  delqueue.enqueue(compute.image_desc_layout, vk.device());

  desc_alloc.alloc_sets(compute.image_desc_layout, &compute.image_desc, 1).value();
  VkDescriptorImageInfo img_info{};
  img_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  img_info.imageView = target.view();
  VkWriteDescriptorSet draw_image_write{};
  draw_image_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  draw_image_write.dstSet = compute.image_desc;
  draw_image_write.dstBinding = 0;
  draw_image_write.descriptorCount = 1;
  draw_image_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  draw_image_write.pImageInfo = &img_info;
  vkUpdateDescriptorSets(vk.device(), 1, &draw_image_write, 0, nullptr);

  render::VkPipelineLayoutBuilder layout_builder;
  compute.layout =
    layout_builder.add_layout(compute.image_desc_layout)
      .add_push_range(VK_SHADER_STAGE_COMPUTE_BIT, sizeof(RenderContext::ComputeConstants), 0)
      .build(vk)
      .value();
  delqueue.enqueue(compute.layout, vk.device());

  compute.pipelines[0] =
    render::vk_create_compute_pipeline(vk, compute.layout, shader_gradient).value();
  delqueue.enqueue(compute.pipelines[0], vk.device());

  compute.pipelines[1] =
    render::vk_create_compute_pipeline(vk, compute.layout, shader_sky).value();
  delqueue.enqueue(compute.pipelines[1], vk.device());

  compute.data[0].data1 = ran::Vec4f32(1.f, 0.f, 0.f, 1.f);
  compute.data[0].data2 = ran::Vec4f32(0.f, 0.f, 1.f, 1.f);
  compute.data[1].data1 = ran::Vec4f32(.1f, .2f, .4f, .97f);
}

constexpr auto rect_verts = std::to_array<RenderContext::Vertex>({
  {
    {.5f, -.5f, 0.f},
    0.f,
    {0.f, 1.f, 0.f},
    1.f,
    {0.f, 1.f, 0.f, 1.f},
  },
  {
    {.5f, .5f, 0.f},
    0.f,
    {0.f, 1.f, 0.f},
    1.f,
    {.5f, .5f, .5f, 1.f},
  },
  {
    {-.5f, -.5f, 0.f},
    0.f,
    {0.f, 1.f, 0.f},
    1.f,
    {1.f, 0.f, 0.f, 1.f},
  },
  {
    {-.5f, .5f, 0.f},
    0.f,
    {0.f, 1.f, 0.f},
    1.f,
    {0.f, 1.f, 0.f, 1.f},
  },
});
constexpr auto rect_indices = std::to_array<u32>({0, 1, 2, 2, 1, 3});

fn copy_buffers(VkContext& vk, VkAllocBuff& vb, const void* vb_data, VkDeviceSize vb_size,
                VkAllocBuff& ib, const void* ib_data, VkDeviceSize ib_size) -> void {
  const render::VkBufferArgs staging_args{
    .size = vb_size + ib_size,
    .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    .mem_usage = render::KA_VK_MEM_USAGE_CPU_ONLY,
  };
  auto staging = render::VkAllocBuff::allocate(vk, staging_args).value();
  const DeferFn staging_defer = [&]() {
    render::vk_dealloc_buffer(vk, staging);
  };

  void* data = staging.mapped_data();
  std::memcpy(data, vb_data, vb_size);
  std::memcpy(((u8*)data) + vb_size, ib_data, ib_size);

  render::vk_submit_immediate(vk, [&](VkCommandBuffer cmd) -> void {
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

fn init_meshes(VkContext& vk, VkDelQueue& delqueue, VkAllocImage& target,
               RenderContext::MeshRenderData& mesh) -> void {
  VkShaderModule triangle_frag = VK_NULL_HANDLE, triangle_vert = VK_NULL_HANDLE,
                 mesh_vert = VK_NULL_HANDLE;
  const DeferFn shader_defer = [&]() {
    render::vk_destroy_shader(vk, triangle_frag);
    render::vk_destroy_shader(vk, triangle_vert);
    render::vk_destroy_shader(vk, mesh_vert);
  };

  const auto vert_src = load_entire_file(KA_RES_DIR "/shaders/colored_triangle.vert.spv");
  triangle_vert = render::vk_create_shader(vk, {vert_src.data(), vert_src.size()}).value();

  const auto frag_src = load_entire_file(KA_RES_DIR "/shaders/colored_triangle.frag.spv");
  triangle_frag = render::vk_create_shader(vk, {frag_src.data(), frag_src.size()}).value();

  const auto mesh_vert_src = load_entire_file(KA_RES_DIR "/shaders/colored_mesh.vert.spv");
  mesh_vert = render::vk_create_shader(vk, {mesh_vert_src.data(), mesh_vert_src.size()}).value();

  // Triangle
  {
    render::VkPipelineLayoutBuilder layout_builder;
    mesh.triangle_layout = layout_builder.build(vk).value();
    delqueue.enqueue(mesh.triangle_layout, vk.device());

    render::VkGfxPipelineBuilder pipeline_builder;
    mesh.triangle_pipeline = pipeline_builder.set_layout(mesh.triangle_layout)
                               .add_module(VK_SHADER_STAGE_VERTEX_BIT, triangle_vert)
                               .add_module(VK_SHADER_STAGE_FRAGMENT_BIT, triangle_frag)
                               .set_poly_mode(VK_POLYGON_MODE_FILL)
                               .set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                               .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
                               .set_color_format(target.format())
                               .set_depth_format(VK_FORMAT_UNDEFINED)
                               .disable_multisampling()
                               .disable_depth_test()
                               .disable_blending()
                               .build(vk)
                               .value();
    delqueue.enqueue(mesh.triangle_pipeline, vk.device());
  }

  // Meshes
  {
    // Mesh vertex buffer
    static constexpr auto vb_size = sizeof(rect_verts);
    const render::VkBufferArgs vb_args{
      .size = vb_size,
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      .mem_usage = render::KA_VK_MEM_USAGE_GPU_ONLY,
    };
    auto vb = render::VkAllocBuff::allocate(vk, vb_args).value();
    delqueue.enqueue(vb, vk.allocator());

    // Mesh index buffer
    static constexpr auto ib_size = sizeof(rect_indices);
    const render::VkBufferArgs ib_args{
      .size = ib_size,
      .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .mem_usage = render::KA_VK_MEM_USAGE_GPU_ONLY,
    };
    auto ib = render::VkAllocBuff::allocate(vk, ib_args).value();
    delqueue.enqueue(ib, vk.allocator());

    copy_buffers(vk, vb, rect_verts.data(), vb_size, ib, rect_indices.data(), ib_size);
    mesh.vertex_buffer.construct(std::move(vb));
    mesh.index_buffer.construct(std::move(ib));

    render::VkPipelineLayoutBuilder layout_builder;
    mesh.mesh_layout =
      layout_builder
        .add_push_range(VK_SHADER_STAGE_VERTEX_BIT, sizeof(RenderContext::MeshConstants), 0)
        .build(vk)
        .value();
    delqueue.enqueue(mesh.mesh_layout, vk.device());

    render::VkGfxPipelineBuilder pipeline_builder;
    mesh.mesh_pipeline = pipeline_builder.set_layout(mesh.mesh_layout)
                           .add_module(VK_SHADER_STAGE_VERTEX_BIT, mesh_vert)
                           .add_module(VK_SHADER_STAGE_FRAGMENT_BIT, triangle_frag)
                           .set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                           .set_poly_mode(VK_POLYGON_MODE_FILL)
                           .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
                           .set_color_format(target.format())
                           .set_depth_format(VK_FORMAT_UNDEFINED)
                           .disable_multisampling()
                           .disable_blending()
                           .disable_blending()
                           .build(vk)
                           .value();
    delqueue.enqueue(mesh.mesh_pipeline, vk.device());
    mesh.quad_transform = ran::Mat4f32::identity();
  }
}

} // namespace

fn RenderContext::create(GLFWContext& glfw) -> VkMsgExpect<RenderContext> {
  VkExtent2D surface_extent{};
  fn extent_updater = glfw.make_vk_extent_updater();
  extent_updater(&surface_extent);
  const render::VkSurfaceArgs vk_surface{
    .extensions = render::GLFWContext::get_surface_extensions(),
    .create = glfw.make_vk_surface_creator(),
    .update_extent = {in_place, extent_updater},
    .swapchain_extent = surface_extent,
  };
  const render::VkContextArgs vk_args{
    .surface = {in_place, std::move(vk_surface)},
    .app_name = KA_APP_NAME,
    .app_ver = KA_APP_VERSION,
  };
  auto vk = render::VkContext::create(vk_args);
  if (!vk) {
    return {unexpect, vk.error()};
  }
  DeferFn vk_defer = make_vk_defer(*vk);

  fn glfw_imgui = glfw.make_imgui_handler();
  fn imgui_initer = [&]() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    glfw_imgui.init();
  };
  render::vk_init_imgui(*vk, imgui_initer);
  DeferFn imgui_defer = make_imgui_defer(glfw_imgui);

  VkDelQueue delqueue;
  DeferFn delqueue_defer = make_delqueue_defer(*vk, delqueue);

  static constexpr auto desc_pool_sizes = std::to_array<render::VkDescPoolRatio>({
    {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
  });
  static constexpr render::VkDescAllocArgs alloc_args{
    .max_sets = 10,
    .ratios = desc_pool_sizes,
  };
  auto desc_alloc = VkDescAlloc::create(vk->device(), alloc_args).value();
  desc_alloc.add_to_delqueue(delqueue);

  const render::VkImageArgs target_args{
    .extent = {surface_extent.width, surface_extent.height, 1},
    .format = VK_FORMAT_R16G16B16A16_SFLOAT,
  };
  auto target = VkAllocImage::allocate(*vk, target_args).value();
  delqueue.enqueue(target, vk->device(), vk->allocator());

  ComputeRenderData compute{};
  init_compute(*vk, delqueue, desc_alloc, target, compute);

  MeshRenderData mesh{};
  init_meshes(*vk, delqueue, target, mesh);

  delqueue_defer.disengage();
  imgui_defer.disengage();
  vk_defer.disengage();
  return {in_place,
          create_t(),
          *std::move(vk),
          std::move(glfw_imgui),
          std::move(delqueue),
          std::move(target),
          std::move(desc_alloc),
          std::move(compute),
          std::move(mesh)};
}

RenderContext::RenderContext(create_t, VkContext&& vk, GLFWContext::ImGuiHandler&& glfw_imgui,
                             VkDelQueue&& delqueue, VkAllocImage&& target,
                             VkDescAlloc&& desc_alloc, ComputeRenderData&& compute,
                             MeshRenderData&& mesh) :
    _vk(std::move(vk)), _glfw_imgui(std::move(glfw_imgui)), _delqueue(std::move(delqueue)),
    _target(std::move(target)), _desc_alloc(std::move(desc_alloc)), _compute(std::move(compute)),
    _mesh(std::move(mesh)) {}

fn RenderContext::destroy() -> void {
  make_delqueue_defer(_vk, _delqueue)();
  make_imgui_defer(_glfw_imgui)();
  make_vk_defer(_vk)();
}

namespace {

fn draw_imgui(GLFWContext::ImGuiHandler& glfw_imgui, RenderContext::ComputeRenderData& compute,
              Vec<RenderContext::MeshAsset>& meshes, VkCommandBuffer cmd,
              VkImageView swapchain_view, VkExtent2D swapchain_extent) {
  static constexpr const char* compute_names[2] = {"gradient_color", "sky"};
  glfw_imgui.new_frame();
  ImGui::NewFrame();

  ImGui::ShowDemoWindow();

  if (ImGui::Begin("background")) {
    ImGui::Text("Selected effect: %s", compute_names[compute.effect_idx]);
    ImGui::SliderInt("Effect Index", &compute.effect_idx, 0, 1);
    ImGui::InputFloat4("data1", compute.data[compute.effect_idx].data1.data());
    ImGui::InputFloat4("data2", compute.data[compute.effect_idx].data2.data());
    ImGui::InputFloat4("data3", compute.data[compute.effect_idx].data3.data());
    ImGui::InputFloat4("data4", compute.data[compute.effect_idx].data4.data());
  }
  ImGui::End();

  if (ImGui::Begin("meshes")) {
    for (const auto& mesh : meshes) {
      ImGui::Text("Mesh: %s", mesh.name.data());
    }
  }
  ImGui::End();

  ImGui::Render();

  const auto color_attachment =
    render::vkmk_attach_info(swapchain_view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  const auto render_info = render::vkmk_render_info(swapchain_extent, &color_attachment, nullptr);
  vkCmdBeginRendering(cmd, &render_info);
  render::vk_draw_imgui(cmd, ImGui::GetDrawData());
  vkCmdEndRendering(cmd);
}

fn draw_compute(const RenderContext::ComputeRenderData& compute, VkExtent2D draw_extent,
                VkCommandBuffer cmd) -> void {
  auto& data = compute.data[compute.effect_idx];
  const auto pipeline = compute.pipelines[compute.effect_idx];
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compute.layout, 0, 1,
                          &compute.image_desc, 0, nullptr);
  vkCmdPushConstants(cmd, compute.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(data), &data);
  vkCmdDispatch(cmd, std::ceil(draw_extent.width / 16.f), std::ceil(draw_extent.height / 16.f), 1);
}

fn draw_geometry(const RenderContext::MeshRenderData& mesh, Vec<RenderContext::MeshAsset>& meshes,
                 VkDevice device, VkImageView target_view, VkExtent2D draw_extent,
                 VkCommandBuffer cmd) -> void {
  const auto color_attachment =
    render::vkmk_attach_info(target_view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  const auto render_info = render::vkmk_render_info(draw_extent, &color_attachment, nullptr);

  // Dynamic state
  VkViewport viewport{};
  viewport.x = 0;
  viewport.y = 0;
  viewport.width = draw_extent.width;
  viewport.height = draw_extent.height;
  viewport.minDepth = 0.f;
  viewport.maxDepth = 1.f;
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent = draw_extent;
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  vkCmdBeginRendering(cmd, &render_info);

  // Draw triangle
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh.triangle_pipeline);
  vkCmdDraw(cmd, 3, 1, 0, 0);

  // Draw quad mesh
  {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh.mesh_pipeline);
    RenderContext::MeshConstants push_constants;
    push_constants.world_mat = mesh.quad_transform;
    push_constants.vertex_buffer = mesh.vertex_buffer->addr(device);
    vkCmdPushConstants(cmd, mesh.mesh_layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(push_constants), &push_constants);
    vkCmdBindIndexBuffer(cmd, mesh.index_buffer->buffer(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, (u32)rect_indices.size(), 1, 0, 0, 0);
  }

  // Draw other meshes
  for (const auto& model_mesh : meshes) {
    RenderContext::MeshConstants push_constants;
    push_constants.world_mat = model_mesh.transform;
    push_constants.vertex_buffer = model_mesh.vertex_buffer.addr(device);
    vkCmdPushConstants(cmd, mesh.mesh_layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(push_constants), &push_constants);
    vkCmdBindIndexBuffer(cmd, model_mesh.index_buffer.buffer(), model_mesh.index_start,
                         VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, model_mesh.index_count, 1, 0, 0, 0);
  }

  vkCmdEndRendering(cmd);
}

} // namespace

fn RenderContext::on_render(f64 dt, f64 alpha) -> void {
  KA_UNUSED(dt);
  KA_UNUSED(alpha);
  render::vk_draw_frame(_vk, [&](const render::VkFrameContext& frame) -> void {
    const auto cmd = frame.cmd;
    const auto draw_extent = [&]() -> VkExtent2D {
      auto ext = _target.extent();
      return {ext.width, ext.height};
    }();

    // 1. Draw using compute pipeline (we use a general layout)
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED; // Don't care about the older layout
    layout = render::vkcmd_transition_image(cmd, _target.image(), layout, VK_IMAGE_LAYOUT_GENERAL);
    draw_compute(_compute, draw_extent, cmd);

    // 2. Draw using graphics pipelines (we use a color attachment layout)
    layout = render::vkcmd_transition_image(cmd, _target.image(), layout,
                                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    draw_geometry(_mesh, _meshes, _vk.device(), _target.view(), draw_extent, cmd);

    // 3. Prepare swapchain for copying
    layout = render::vkcmd_transition_image(cmd, _target.image(), layout,
                                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    VkImageLayout sw_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    sw_layout = render::vkcmd_transition_image(cmd, frame.swapchain_image, sw_layout,
                                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // 4. Do the copy
    render::vkcmd_transfer_image(cmd, _target.image(), frame.swapchain_image, draw_extent,
                                 frame.swapchain_extent);

    // 5. Draw ImGui ontop of the swapchain image (not the render image!!!)
    sw_layout = render::vkcmd_transition_image(cmd, frame.swapchain_image, sw_layout,
                                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    draw_imgui(_glfw_imgui, _compute, _meshes, cmd, frame.swapchain_view, frame.swapchain_extent);

    // 6. Prepare swapchain image for presenting
    sw_layout = render::vkcmd_transition_image(cmd, frame.swapchain_image, sw_layout,
                                               VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  }).value();
}

fn RenderContext::on_fixed_update(u32 ups) -> void {
  KA_UNUSED(ups);
  static constexpr auto delta = ran::pi<f32> / 60.f;
  _mesh.quad_transform = ran::rotate(_mesh.quad_transform, delta, ran::Vec3f32(0.f, 0.f, 1.f));
}

namespace {

fn soa_to_aos(Span<const ran::Vec3f32> pos, Span<const ran::Vec2f32> uvs)
  -> UniqueArray<RenderContext::Vertex> {
  const usize count = pos.size();
  ka_assert(uvs.size() == count);
  auto out = make_array<RenderContext::Vertex>(uninitialized, count);
  for (usize i = 0; i < count; ++i) {
    out[i].color = ran::Vec4f32(.25f, .25f, .25f, 1.f); // make something up
    out[i].uv_x = uvs[i].x;
    out[i].uv_y = uvs[i].y;
    out[i].pos = pos[i];
  }
  return out;
}

} // namespace

fn RenderContext::add_mesh(const MeshData& mesh, std::string_view name) -> void {
  log_debug(" Adding mesh: {}", name);
  // Vertex buffer
  const auto vertices = soa_to_aos(mesh.positions, mesh.uvs);
  const auto vb_size = vertices.size() * sizeof(vertices[0]);
  const render::VkBufferArgs vb_args{
    .size = vb_size,
    .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
             VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    .mem_usage = render::KA_VK_MEM_USAGE_GPU_ONLY,
  };
  auto vb = render::VkAllocBuff::allocate(_vk, vb_args).value();
  _delqueue.enqueue(vb, _vk.allocator());

  // Index buffer
  const auto ib_size = mesh.indices.size_bytes();
  const render::VkBufferArgs ib_args{
    .size = ib_size,
    .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    .mem_usage = render::KA_VK_MEM_USAGE_GPU_ONLY,
  };
  auto ib = render::VkAllocBuff::allocate(_vk, ib_args).value();
  _delqueue.enqueue(ib, _vk.allocator());

  copy_buffers(_vk, vb, vertices.data(), vb_size, ib, mesh.indices.data(), ib_size);
  _meshes.emplace_back(ran::Mat4f32::identity(), std::move(vb), std::move(ib), 0,
                       (u32)mesh.indices.size(), name);
}

} // namespace kappa::render
