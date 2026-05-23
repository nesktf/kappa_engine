#include "./render/vulkan/vk_buffer.hpp"
#include "./render/vulkan/vk_context.hpp"
#include "./render/vulkan/vk_imgui.hpp"
#include "./render/vulkan/vk_pipeline.hpp"
#include "./render/vulkan/vk_util.hpp"

#include "./render/glfw.hpp"

#include "./util/filesystem.hpp"
#include "./util/logger.hpp"

#include <ranmath/ran.hpp>

#define KA_APP_NAME    "Kappa"
#define KA_APP_VERSION VK_MAKE_VERSION(KA_VER_MAJ, KA_VER_MIN, KA_VER_REV)

namespace {

using namespace kappa;

struct ComputeConstants {
  ran::Vec4f32 data1;
  ran::Vec4f32 data2;
  ran::Vec4f32 data3;
  ran::Vec4f32 data4;
};

struct Vertex {
  ran::Vec3f32 pos;
  f32 uv_x;
  ran::Vec3f32 normal;
  f32 uv_y;
  ran::Vec4f32 color;
};

struct GFXConstants {
  ran::Mat4f32 world_mat;
  VkDeviceAddress vertex_buffer;
};

constexpr u32 WINDOW_WIDTH = 1280;
constexpr u32 WINDOW_HEIGHT = 720;

constexpr auto rect_verts = std::to_array<Vertex>({
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

fn run_engine() -> void {
  auto glfw = render::GLFWContext::create(WINDOW_WIDTH, WINDOW_HEIGHT);
  const DeferFn glfw_defer = [&]() {
    glfw.destroy();
  };

  VkExtent2D surface_extent{};
  auto extent_updater = glfw.make_vk_extent_updater();
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
  auto vk = render::VkContext::create(vk_args).value();
  const DeferFn vk_defer = [&]() {
    render::vk_destroy_context(vk);
  };

  auto imgui = glfw.make_imgui_initer();
  render::vk_init_imgui(vk, imgui);
  const DeferFn imgui_defer = [&]() {
    render::vk_shutdown_imgui();
    imgui.destroy();
    ImGui::DestroyContext();
  };

  render::VkDelQueue delqueue;
  const DeferFn delqueue_defer = [&]() {
    vkDeviceWaitIdle(vk.device());
    delqueue.flush();
  };

  const auto desc_pool_sizes = std::to_array<render::VkDescPoolRatio>({
    {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
  });
  const render::VkDescAllocArgs alloc_args{
    .max_sets = 10,
    .ratios = desc_pool_sizes,
  };
  auto desc_alloc = render::VkDescAlloc::create(vk.device(), alloc_args).value();
  desc_alloc.add_to_delqueue(delqueue);

  // Draw image target
  const render::VkImageArgs target_args{
    .extent = {WINDOW_WIDTH, WINDOW_HEIGHT, 1},
    .format = VK_FORMAT_R16G16B16A16_SFLOAT,
  };
  auto target = render::VkAllocImage::allocate(vk, target_args).value();
  delqueue.enqueue(target, vk.device(), vk.allocator());

  // Compute effects
  static constexpr const char* compute_names[2] = {"gradient_color", "sky"};
  ComputeConstants compute_data[2]{};
  compute_data[0].data1 = ran::Vec4f32(1.f, 0.f, 0.f, 1.f);
  compute_data[0].data2 = ran::Vec4f32(0.f, 0.f, 1.f, 1.f);
  compute_data[1].data1 = ran::Vec4f32(.1f, .2f, .4f, .97f);

  // Compute pipelines
  VkDescriptorSetLayout image_desc_layout = VK_NULL_HANDLE;
  VkDescriptorSet image_desc = VK_NULL_HANDLE;
  VkPipelineLayout compute_layout = VK_NULL_HANDLE;
  VkPipeline compute_pipelines[2] = {0};
  {
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
    image_desc_layout = desc_layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                          .build(vk, VK_SHADER_STAGE_COMPUTE_BIT)
                          .value();
    delqueue.enqueue(image_desc_layout, vk.device());

    desc_alloc.alloc_sets(image_desc_layout, &image_desc, 1).value();
    VkDescriptorImageInfo img_info{};
    img_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    img_info.imageView = target.view();
    VkWriteDescriptorSet draw_image_write{};
    draw_image_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    draw_image_write.dstSet = image_desc;
    draw_image_write.dstBinding = 0;
    draw_image_write.descriptorCount = 1;
    draw_image_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    draw_image_write.pImageInfo = &img_info;
    vkUpdateDescriptorSets(vk.device(), 1, &draw_image_write, 0, nullptr);

    render::VkPipelineLayoutBuilder layout_builder;
    compute_layout = layout_builder.add_layout(image_desc_layout)
                       .add_push_range(VK_SHADER_STAGE_COMPUTE_BIT, sizeof(ComputeConstants), 0)
                       .build(vk)
                       .value();
    delqueue.enqueue(compute_layout, vk.device());

    compute_pipelines[0] =
      render::vk_create_compute_pipeline(vk, compute_layout, shader_gradient).value();
    delqueue.enqueue(compute_pipelines[0], vk.device());

    compute_pipelines[1] =
      render::vk_create_compute_pipeline(vk, compute_layout, shader_sky).value();
    delqueue.enqueue(compute_pipelines[1], vk.device());
  }

  // Triangle pipeline
  VkPipelineLayout triangle_layout = VK_NULL_HANDLE;
  VkPipeline triangle_pipeline = VK_NULL_HANDLE;
  {
    VkShaderModule triangle_frag = VK_NULL_HANDLE, triangle_vert = VK_NULL_HANDLE;
    const DeferFn shader_defer = [&]() {
      render::vk_destroy_shader(vk, triangle_frag);
      render::vk_destroy_shader(vk, triangle_vert);
    };

    const auto vert_src = load_entire_file(KA_RES_DIR "/shaders/colored_triangle.vert.spv");
    triangle_vert = render::vk_create_shader(vk, {vert_src.data(), vert_src.size()}).value();

    const auto frag_src = load_entire_file(KA_RES_DIR "/shaders/colored_triangle.frag.spv");
    triangle_frag = render::vk_create_shader(vk, {frag_src.data(), frag_src.size()}).value();

    render::VkPipelineLayoutBuilder layout_builder;
    triangle_layout = layout_builder.build(vk).value();
    delqueue.enqueue(triangle_layout, vk.device());

    render::VkGfxPipelineBuilder pipeline_builder;
    triangle_pipeline = pipeline_builder.set_layout(triangle_layout)
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
    delqueue.enqueue(triangle_pipeline, vk.device());
  }

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
    .size = vb_size,
    .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    .mem_usage = render::KA_VK_MEM_USAGE_GPU_ONLY,
  };
  auto ib = render::VkAllocBuff::allocate(vk, ib_args).value();
  delqueue.enqueue(ib, vk.allocator());

  // Copy using staging buffer
  {
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
    std::memcpy(data, rect_verts.data(), vb_size);
    std::memcpy(((u8*)data) + vb_size, rect_indices.data(), ib_size);
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

  // Mesh pipeline
  VkPipelineLayout mesh_layout = VK_NULL_HANDLE;
  VkPipeline mesh_pipeline = VK_NULL_HANDLE;
  {
    VkShaderModule mesh_vert = VK_NULL_HANDLE, mesh_frag = VK_NULL_HANDLE;
    const DeferFn shader_defer = [&]() {
      render::vk_destroy_shader(vk, mesh_vert);
      render::vk_destroy_shader(vk, mesh_frag);
    };

    const auto vert_src = load_entire_file(KA_RES_DIR "/shaders/colored_mesh.vert.spv");
    mesh_vert = render::vk_create_shader(vk, {vert_src.data(), vert_src.size()}).value();

    const auto frag_src = load_entire_file(KA_RES_DIR "/shaders/colored_triangle.frag.spv");
    mesh_frag = render::vk_create_shader(vk, {frag_src.data(), frag_src.size()}).value();

    render::VkPipelineLayoutBuilder layout_builder;
    mesh_layout =
      layout_builder.add_push_range(VK_SHADER_STAGE_VERTEX_BIT, sizeof(GFXConstants), 0)
        .build(vk)
        .value();
    delqueue.enqueue(mesh_layout, vk.device());

    render::VkGfxPipelineBuilder pipeline_builder;
    mesh_pipeline = pipeline_builder.set_layout(mesh_layout)
                      .add_module(VK_SHADER_STAGE_VERTEX_BIT, mesh_vert)
                      .add_module(VK_SHADER_STAGE_FRAGMENT_BIT, mesh_frag)
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
    delqueue.enqueue(mesh_pipeline, vk.device());
  }

  // Draw functions
  s32 compute_idx = 0;
  const fn draw_imgui = [&](VkCommandBuffer cmd, VkImageView swapchain_view,
                            VkExtent2D swapchain_extent) {
    glfw.start_imgui_frame();
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();

    if (ImGui::Begin("background")) {
      ImGui::Text("Selected effect: %s", compute_names[compute_idx]);
      ImGui::SliderInt("Effect Index", &compute_idx, 0, 1);
      ImGui::InputFloat4("data1", compute_data[compute_idx].data1.data());
      ImGui::InputFloat4("data2", compute_data[compute_idx].data2.data());
      ImGui::InputFloat4("data3", compute_data[compute_idx].data3.data());
      ImGui::InputFloat4("data4", compute_data[compute_idx].data4.data());
    }
    ImGui::End();

    ImGui::Render();

    const auto color_attachment =
      render::vkmk_attach_info(swapchain_view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const auto render_info =
      render::vkmk_render_info(swapchain_extent, &color_attachment, nullptr);
    vkCmdBeginRendering(cmd, &render_info);
    render::vk_draw_imgui(cmd, ImGui::GetDrawData());
    vkCmdEndRendering(cmd);
  };

  const auto draw_extent = [&]() -> VkExtent2D {
    auto ext = target.extent();
    return {ext.width, ext.height};
  }();
  const fn draw_compute = [&](VkCommandBuffer cmd) -> void {
    auto& data = compute_data[compute_idx];
    const auto pipeline = compute_pipelines[compute_idx];
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compute_layout, 0, 1, &image_desc,
                            0, nullptr);
    vkCmdPushConstants(cmd, compute_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(data), &data);
    vkCmdDispatch(cmd, std::ceil(draw_extent.width / 16.f), std::ceil(draw_extent.height / 16.f),
                  1);
  };

  u32 curr_frame = 0;
  const fn clear_background = [&](VkCommandBuffer cmd) -> void {
    // make a clear-color from frame number. This will flash with a 120 frame period.
    f32 flash = std::abs(std::sin((f32)curr_frame / 120.f));
    VkClearColorValue clear_value{{0.0f, 0.0f, flash, 1.0f}};
    const auto clear_range = render::vkmk_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
    // clear image
    vkCmdClearColorImage(cmd, target.image(), VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1,
                         &clear_range);
  };
  KA_UNUSED(clear_background);

  // Draw geometry
  auto transform = ran::Mat4f32::identity();
  const fn draw_geometry = [&](VkCommandBuffer cmd) -> void {
    const auto color_attachment =
      render::vkmk_attach_info(target.view(), nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
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
    {
      // Draw triangle
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, triangle_pipeline);
      vkCmdDraw(cmd, 3, 1, 0, 0);

      // Draw mesh
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline);
      GFXConstants push_constants;
      push_constants.world_mat = transform;
      push_constants.vertex_buffer = vb.addr(vk);
      vkCmdPushConstants(cmd, mesh_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constants),
                         &push_constants);
      vkCmdBindIndexBuffer(cmd, ib.buffer(), 0, VK_INDEX_TYPE_UINT32);
      vkCmdDrawIndexed(cmd, (u32)rect_indices.size(), 1, 0, 0, 0);
    }
    vkCmdEndRendering(cmd);
  };
  fn draw_things = [&](const render::VkFrameContext& frame) -> void {
    const auto cmd = frame.cmd;

    // 1. Draw using compute pipeline (we use a general layout)
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED; // Don't care about the older layout
    layout = render::vkcmd_transition_image(cmd, target.image(), layout, VK_IMAGE_LAYOUT_GENERAL);
    draw_compute(cmd);

    // 2. Draw using graphics pipelines (we use a color attachment layout)
    layout = render::vkcmd_transition_image(cmd, target.image(), layout,
                                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    draw_geometry(cmd);

    // 3. Prepare swapchain for copying
    layout = render::vkcmd_transition_image(cmd, target.image(), layout,
                                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    VkImageLayout sw_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    sw_layout = render::vkcmd_transition_image(cmd, frame.swapchain_image, sw_layout,
                                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // 4. Do the copy
    render::vkcmd_transfer_image(cmd, target.image(), frame.swapchain_image, draw_extent,
                                 frame.swapchain_extent);

    // 5. Draw ImGui ontop of the swapchain image (not the render image!!!)
    sw_layout = render::vkcmd_transition_image(cmd, frame.swapchain_image, sw_layout,
                                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    draw_imgui(cmd, frame.swapchain_view, frame.swapchain_extent);

    // 6. Prepare swapchain image for presenting
    sw_layout = render::vkcmd_transition_image(cmd, frame.swapchain_image, sw_layout,
                                               VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  };

  const fn on_render = [&](f64 dt, f64 alpha) {
    KA_UNUSED(dt);
    KA_UNUSED(alpha);

    render::vk_draw_frame(vk, draw_things).value();
    ++curr_frame;
  };

  const fn on_fixed_update = [&](u32 ups) {
    KA_UNUSED(ups);
    static constexpr auto delta = ran::pi<f32> / 60.f;
    transform = ran::rotate(transform, delta, ran::Vec3f32(0.f, 0.f, 1.f));
  };

  render::render_loop<60>(glfw, OverloadFn{on_fixed_update, on_render});
}

} // namespace

int kappa::g_argc;
char** kappa::g_argv;

int main(int argc, char* argv[]) {
  g_argc = argc;
  g_argv = argv;
  try {
    run_engine();
  } catch (const std::exception& ex) {
    log_error(" {}", ex.what());
  }
}
