#pragma once

#include "./vk.h"

#include "./vk_error.hpp"

#include "../../util/function.hpp"
#include "../../util/ptr.hpp"

#include <type_traits>

#ifdef KA_VK_ENABLE_IMGUI
#include <imgui_impl_vulkan.h>
#endif

namespace kappa::render {

struct VulkanContextArgs {
public:
  using ImGuiInitFn = FnRef<void()>;
  using SurfaceCreateFn =
    FnRef<VkResult(VkInstance vk, VkSurfaceKHR* surface, const VkAllocationCallbacks* vkalloc)>;

public:
  VkExtent2D initial_extent;
  Span<const char*> surface_extensions;
  SurfaceCreateFn create_surface_fn;
  Optional<ImGuiInitFn> init_imgui_fn;
  const char* app_name;
  u32 app_ver;
};

class VulkanContext {
private:
  struct Deleter {
    void operator()(ka_VulkanContext ctx) noexcept { ka_vk_destroy_context(ctx); }
  };

  struct create_t {};

public:
  using ImSubmitFn = TrivFn<void(VkCommandBuffer), 4 * sizeof(void*), 8>;

public:
  VulkanContext(create_t, ka_VulkanContext ctx) noexcept : _ctx(ctx) {}

public:
  static fn create(const VulkanContextArgs& args) noexcept -> VkMsgExpect<VulkanContext>;

public:
  fn rebuild_swapchain(VkExtent2D surface_extent) noexcept -> VkExpect<void>;

  fn draw() noexcept -> VkExpect<void>;

  template<typename F>
  fn immediate_submit(F&& func) -> VkExpect<void>;

public:
  operator ka_VulkanContext() noexcept { return _ctx.get(); }

private:
  std::unique_ptr<std::remove_pointer_t<ka_VulkanContext>, Deleter> _ctx;
};

using VulkanImageArgs = ka_VulkanImageArgs;

class VulkanImage : public ka_VulkanImage {
private:
  struct create_t {};

public:
  VulkanImage(create_t, ka_VulkanImage&& image) noexcept : ka_VulkanImage(image) {}

public:
  static fn allocate(ka_VulkanContext vk, const VulkanImageArgs& args) -> VkExpect<VulkanImage>;
  fn destroy() -> void;

public:
  fn extent() -> VkExtent2D;
  fn format() -> VkFormat;
};

using VulkanBufferArgs = ka_VulkanBufferArgs;

class VulkanBuffer : public ka_VulkanBuffer {
private:
  struct create_t {};

public:
  VulkanBuffer(create_t, ka_VulkanBuffer&& buffer) noexcept : ka_VulkanBuffer(buffer) {}

public:
  static fn allocate(ka_VulkanContext vk, const VulkanBufferArgs& args) -> VkExpect<VulkanBuffer>;
  fn destroy() -> void;

public:
  fn size() -> usize;
  fn mapped_data() -> void*;
};

#ifdef KA_VK_ENABLE_IMGUI
template<typename F>
fn vk_init_imgui(ka_VulkanContext vk, F&& imgui_initer) -> VkResult;

struct VulkanImGuiDrawData {
  ImDrawData* draw_data;
  VkPipeline pipeline;
};

fn vk_draw_imgui(void* user, VkCommandBuffer cmd);

fn vk_shutown_imgui() -> void;
#endif

} // namespace kappa::render

namespace kappa::render {

#if 0
inline fn VulkanContext::create(const VulkanContextArgs& args) noexcept
  -> VkMsgExpect<VulkanContext> {
  ka_VulkanContextArgs c_args{};
  c_args.app_name = args.app_name;
  c_args.app_ver = args.app_ver;

  fn create_surface = [&](VkInstance vk, VkSurfaceKHR* surface,
                          const VkAllocationCallbacks* vkalloc) -> VkResult {
    return args.create_surface_fn(vk, surface, vkalloc);
  };
  const fn create_surface_trampoline = +[](void* user, VkInstance vk, VkSurfaceKHR* surface,
                                           const VkAllocationCallbacks* vkalloc) -> VkResult {
    return (*(decltype(create_surface)*)user)(vk, surface, vkalloc);
  };
  c_args.create_surface = create_surface_trampoline;
  c_args.create_surface_user = &create_surface;
  c_args.surface_extensions = args.surface_extensions.data();
  c_args.surface_extension_count = args.surface_extensions.size();

  fn init_imgui = [&]() -> void {
    (*args.init_imgui_fn)();
  };
  const fn init_imgui_trampoline = +[](void* user) -> void {
    (*(decltype(init_imgui)*)user)();
  };
  if (args.init_imgui_fn) {
    c_args.init_imgui = init_imgui_trampoline;
    c_args.init_imgui_user = &init_imgui;
  }

  ka_VulkanContext ctx;
  const char* msg;
  if (auto ret = ka_vk_create_context(&ctx, &c_args, &msg)) {
    return {unexpect, msg, ret};
  }
  return {in_place, create_t(), ctx};
}

inline fn VulkanContext::rebuild_swapchain(VkExtent2D surface_extent) noexcept -> VkExpect<void> {
  if (auto ret = ka_vk_rebuild_swapchain(_ctx.get(), surface_extent)) {
    return {unexpect, ret};
  }
  return {};
}

template<typename F>
fn VulkanContext::immediate_submit(F&& func) -> VkExpect<void> {
  static_assert(std::is_invocable_v<std::remove_cvref_t<F>, VkCommandBuffer>,
                "F is not invocable");
  ka_VulkanImSubmitData c_data{};

  fn submit = [&](VkCommandBuffer cmd) {
    func(cmd);
  };
  const fn submit_trampoline = +[](void* user, VkCommandBuffer cmd) -> void {
    (*(decltype(submit)*)user)(cmd);
  };
  c_data.submit_callback_user = &submit;
  c_data.submit_callback = submit_trampoline;

  if (auto ret = ka_vk_immediate_submit(_ctx.get(), &c_data)) {
    return {unexpect, ret};
  }
  return {};
}
#endif

#ifdef KA_VK_ENABLE_IMGUI
template<typename F>
fn vk_init_imgui(ka_VulkanContext vk, F&& imgui_initer) -> VkResult {
  static constexpr auto pool_sizes =
    std::to_array<VkDescriptorPoolSize>({{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
                                         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                                         {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
                                         {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}});

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 1000;
  pool_info.poolSizeCount = (u32)pool_sizes.size();
  pool_info.pPoolSizes = pool_sizes.data();
  ka_VulkanImGuiInfo imgui_info;
  VkResult ret = VK_SUCCESS;
  if (ret = ka_vk_create_imgui_info(vk, &imgui_info, &pool_info); ret != VK_SUCCESS) {
    return ret;
  }

  ImGui_ImplVulkan_InitInfo init_info{};
  init_info.Instance = imgui_info.vk;
  init_info.PhysicalDevice = imgui_info.physical_device;
  init_info.Device = imgui_info.device;
  init_info.Queue = imgui_info.queue;
  init_info.DescriptorPool = imgui_info.descpool;
  init_info.MinImageCount = 3;
  init_info.ImageCount = 3;
  init_info.UseDynamicRendering = true;

  init_info.PipelineInfoMain.PipelineRenderingCreateInfo.sType =
    VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
  init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats =
    &imgui_info.swapchain_format;
  init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  imgui_initer();
  ImGui_ImplVulkan_Init(&init_info);

  return ret;
}

inline fn vk_shutown_imgui() -> void {
  ImGui_ImplVulkan_Shutdown();
}

inline fn vk_draw_imgui(void* user, VkCommandBuffer cmd) {
  const auto& imgui_data = *static_cast<VulkanImGuiDrawData*>(user);
  ImGui_ImplVulkan_RenderDrawData(imgui_data.draw_data, cmd, imgui_data.pipeline);
}
#endif

} // namespace kappa::render
