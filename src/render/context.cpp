#include "./context.hpp"

#include "./render/vulkan/vk_imgui.hpp"

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
    vk_shutdown_imgui();
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

fn init_draw_target(VkContext& vk, VkDelQueue& delqueue, VkExtent2D surface_extent)
  -> RenderContext::DrawTarget {
  static constexpr auto base_usage =
    VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
  const VkImageArgs color_target_args{
    .extent = {surface_extent.width, surface_extent.height, 1},
    .format = VK_FORMAT_R16G16B16A16_SFLOAT,
    .usage = base_usage | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    .mipmaps = KA_VK_DISABLE_MIPMAPS,
  };
  auto color = VkAllocImage::create(vk.device(), vk.allocator(), color_target_args).value();
  delqueue.enqueue(color, vk.device(), vk.allocator());
  const VkImageArgs depth_target_args{
    .extent = {surface_extent.width, surface_extent.height, 1},
    .format = VK_FORMAT_D32_SFLOAT,
    .usage = base_usage | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    .mipmaps = KA_VK_DISABLE_MIPMAPS,
  };
  auto depth = VkAllocImage::create(vk.device(), vk.allocator(), depth_target_args).value();
  delqueue.enqueue(depth, vk.device(), vk.allocator());
  return {std::move(color), std::move(depth), surface_extent, 1.f};
}

fn create_image(VkContext& vk, const void* data, VkExtent3D size, VkFormat format,
                VkImageUsageFlags flags, VkImageMipsFlag mips) -> VkExpect<VkAllocImage> {
  const size_t data_size = size.depth * size.width * size.height * 4;
  const VkImageArgs image_args{
    .extent = size,
    .format = format,
    .usage = flags | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    .mipmaps = mips,
  };
  auto imag = VkAllocImage::create(vk.device(), vk.allocator(), image_args);
  if (!imag) {
    return imag;
  }
  DeferFn on_err = [&]() {
    vk_destroy_image(vk.device(), vk.allocator(), *imag);
  };

  if (data) {
    const VkBufferArgs upload_args{
      .size = data_size,
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .mem_usage = KA_VK_MEM_USAGE_CPU_TO_GPU,
    };
    auto upload_buff = VkAllocBuff::create(vk.allocator(), upload_args);
    if (!upload_buff) {
      return {unexpect, upload_buff.error()};
    }
    std::memcpy(upload_buff->mapped_data(), data, data_size);
    const DeferFn buff_defer = [&]() {
      vk_destroy_buffer(vk.allocator(), *upload_buff);
    };
    vk_submit_immediate(vk, [&](VkCommandBuffer cmd) -> void {
      auto layout = VK_IMAGE_LAYOUT_UNDEFINED;
      layout =
        vkcmd_transition_image(cmd, imag->image(), layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
      VkBufferImageCopy region{};
      region.bufferOffset = 0;
      region.bufferRowLength = 0;
      region.bufferImageHeight = 0;

      region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      region.imageSubresource.mipLevel = 0;
      region.imageSubresource.baseArrayLayer = 0;
      region.imageSubresource.layerCount = 1;
      region.imageExtent = size;

      vkCmdCopyBufferToImage(cmd, upload_buff->buffer(), imag->image(),
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

      layout = vkcmd_transition_image(cmd, imag->image(), layout,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }).value();
  }
  on_err.disengage();
  return imag;
}

constexpr auto default_texture = []() {
  constexpr u32 magenta = 0xFFFF00FF;
  constexpr u32 black = 0xFF000000;
  std::array<u32, 16 * 16> pixels;
  for (u32 x = 0; x < 16; x++) {
    for (u32 y = 0; y < 16; y++) {
      pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
    }
  }
  return pixels;
}();

fn init_images(VkContext& vk, VkDelQueue& delqueue)
  -> std::pair<VkAllocImage, RenderContext::SamplerArray> {
  auto image =
    create_image(vk, default_texture.data(), VkExtent3D(16, 16, 1), VK_FORMAT_R8G8B8A8_UNORM,
                 VK_IMAGE_USAGE_SAMPLED_BIT, KA_VK_DISABLE_MIPMAPS)
      .value();
  delqueue.enqueue(image, vk.device(), vk.allocator());
  RenderContext::SamplerArray samplers{};
  samplers[RenderContext::SAMPLER_LINEAR] =
    vk_create_sampler(vk.device(), VK_FILTER_LINEAR, VK_FILTER_LINEAR).value();
  delqueue.enqueue(samplers[RenderContext::SAMPLER_LINEAR], vk.device());
  samplers[RenderContext::SAMPLER_NEAREST] =
    vk_create_sampler(vk.device(), VK_FILTER_NEAREST, VK_FILTER_NEAREST).value();
  delqueue.enqueue(samplers[RenderContext::SAMPLER_NEAREST], vk.device());

  return {std::move(image), std::move(samplers)};
}

} // namespace

RenderContext::RenderContext(create_t, VkContext&& vk, GLFWContext::ImGuiHandler&& glfw_imgui,
                             VkDelQueue&& delqueue, VkDynDescAlloc&& desc_alloc,
                             DrawTarget&& target, FrameData* frames, VkAllocImage&& default_image,
                             SamplerArray&& samplers) :
    _vk(std::move(vk)), _glfw_imgui(std::move(glfw_imgui)), _delqueue(std::move(delqueue)),
    _desc_alloc(std::move(desc_alloc)), _target(std::move(target)),
    _images(std::move(default_image), std::move(samplers)), _frame_count(0) {
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    _frames.construct_offset(i, std::move(frames[i]));
  }
}

fn RenderContext::initialize(Uninited<RenderContext> renderer, GLFWContext& glfw) -> void {
  VkExtent2D surface_extent{};
  fn extent_updater = glfw.make_vk_extent_updater();
  extent_updater(&surface_extent);
  const VkSurfaceArgs vk_surface{
    .extensions = GLFWContext::get_surface_extensions(),
    .create = glfw.make_vk_surface_creator(),
    .update_extent = {in_place, extent_updater},
    .swapchain_extent = surface_extent,
  };
  const VkContextArgs vk_args{
    .surface = {in_place, std::move(vk_surface)},
    .app_name = KA_APP_NAME,
    .app_ver = KA_APP_VERSION,
  };
  auto vk = VkContext::create(vk_args).value();
  DeferFn vk_defer = make_vk_defer(vk);

  fn glfw_imgui = glfw.make_imgui_handler();
  fn imgui_initer = [&]() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    glfw_imgui.init();
  };
  vk_init_imgui(vk, imgui_initer);
  DeferFn imgui_defer = make_imgui_defer(glfw_imgui);

  VkDelQueue delqueue;
  DeferFn delqueue_defer = make_delqueue_defer(vk, delqueue);

  static constexpr auto desc_pool_sizes = std::to_array<VkDescPoolRatio>({
    {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
  });
  static constexpr u32 initial_sets = 10;
  auto desc_alloc = VkDynDescAlloc::create(vk.device(), initial_sets, desc_pool_sizes).value();

  auto target = init_draw_target(vk, delqueue, surface_extent);

  RenderContext::FrameArray frames;
  static constexpr auto frame_sizes = std::to_array<VkDescPoolRatio>({
    {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
  });
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    auto alloc = VkDynDescAlloc::create(vk.device(), 1000, frame_sizes).value();
    frames.construct_offset(i, VkDelQueue(), std::move(alloc));
  }

  VkDescriptorSetLayout scene_layout{};
  {
    VkDescLayoutBuilder builder;
    scene_layout = builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                     .build(vk, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                     .value();
  }
  delqueue.enqueue(scene_layout, vk.device());

  auto [default_image, samplers] = init_images(vk, delqueue);

  delqueue_defer.disengage();
  imgui_defer.disengage();
  vk_defer.disengage();

  KA_PNEW(renderer);
  RenderContext(create_t(), std::move(vk), std::move(glfw_imgui), std::move(delqueue),
                std::move(desc_alloc), std::move(target), frames.get(), std::move(default_image),
                std::move(samplers));
}

fn RenderContext::destroy() -> void {
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    auto& frame = _frames[i];
    make_delqueue_defer(_vk, frame.delqueue)();
    frame.desc_alloc.destroy();
  }
  make_delqueue_defer(_vk, _delqueue)();
  _desc_alloc.destroy();
  make_imgui_defer(_glfw_imgui)();
  make_vk_defer(_vk)();
}

namespace {

fn update_draw_target_extent(RenderContext::DrawTarget& target, VkExtent2D swapchain_extent)
  -> void {
  const auto [actual_w, actual_h, _] = target.color.extent();
  target.extent.width = (u32)(std::min(swapchain_extent.width, actual_w) * target.scale);
  target.extent.height = (u32)(std::min(swapchain_extent.height, actual_h) * target.scale);
}

} // namespace

fn RenderContext::draw_things(IDrawAction& draw) {
  vk_draw_frame(_vk, [&](const VkFrameContext& frame) -> void {
    auto& ctx_frame = get_frame();
    ctx_frame.delqueue.flush();
    ctx_frame.desc_alloc.clear();

    const auto cmd = frame.cmd;
    update_draw_target_extent(_target, frame.swapchain_extent);

    VkImageLayout target_layout = VK_IMAGE_LAYOUT_UNDEFINED; // Don't care about the older layout

    // Draw our things
    draw.render_geometry(target_layout, cmd);

    // Prepare swapchain for copying
    target_layout = vkcmd_transition_image(cmd, _target.color.image(), target_layout,
                                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    VkImageLayout sw_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    sw_layout = vkcmd_transition_image(cmd, frame.swapchain_image, sw_layout,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Do the copy
    vkcmd_transfer_image(cmd, _target.color.image(), frame.swapchain_image, _target.extent,
                         frame.swapchain_extent);

    // Draw ImGui ontop of the swapchain image (not the render image!!!)
    sw_layout = vkcmd_transition_image(cmd, frame.swapchain_image, sw_layout,
                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    // Draw imgui
    {
      _glfw_imgui.new_frame();
      ImGui::NewFrame();
      draw.render_imgui(frame);
      ImGui::Render();

      const auto color_attachment =
        vkmk_attach_info(frame.swapchain_view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
      const auto render_info =
        vkmk_render_info(frame.swapchain_extent, &color_attachment, nullptr);
      vkCmdBeginRendering(cmd, &render_info);
      vk_draw_imgui(cmd, ImGui::GetDrawData());
      vkCmdEndRendering(cmd);
    }

    // Prepare swapchain image for presenting
    sw_layout = vkcmd_transition_image(cmd, frame.swapchain_image, sw_layout,
                                       VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    ++_frame_count;
  }).value();
}

#if 0
fn RenderContext::on_fixed_update(u32 ups) -> void {
  KA_UNUSED(ups);
  static constexpr auto delta = ran::pi<f32> / 60.f;
  _mesh.quad_transform = ran::rotate(_mesh.quad_transform, delta, ran::Vec3f32(0.f, 0.f, 1.f));
}
#endif

} // namespace kappa::render
