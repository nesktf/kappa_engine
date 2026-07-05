#pragma once

#include "render/vulkan/vk_common.hpp"

namespace kappa::render {

template<typename Signature>
using VkInplaceFn = TrivFn<Signature, 2 * sizeof(void*), 8>;

using VkUpdateSurfExtFn = VkInplaceFn<void(VkExtent2D*)>;
using VkCreateSurfFn =
  VkInplaceFn<VkResult(VkInstance, VkSurfaceKHR*, const VkAllocationCallbacks*)>;

struct VkSurfaceArgs {
  Span<const char*> extensions;
  VkCreateSurfFn create;
  Optional<VkUpdateSurfExtFn> update_extent;
  VkExtent2D swapchain_extent;
};

struct VkContextArgs {
  Optional<VkSurfaceArgs> surface;
  const char* app_name;
  u32 app_ver;
};

class VkContext {
private:
  struct create_t {};

public:
  VkContext(create_t, VkContext_Impl* vk) noexcept : _vk(vk) {}

  static fn create(const VkContextArgs& args) -> VkMsgExpect<VkContext>;

public:
  fn device() const -> VkDevice;
  fn physical_device() const -> VkPhysicalDevice;
  fn allocator() const -> VkMemAllocator;

public:
  VkContext_Impl& get() { return *_vk; }

  operator VkContext_Impl&() { return *_vk; }

private:
  VkContext_Impl* _vk;
};

fn vk_destroy_context(VkContext_Impl& vk) noexcept -> void;

fn vk_rebuild_swapchain(VkContext_Impl& vk, VkExtent2D extent) -> VkExpect<void>;

struct VkFrameContext {
  VkCommandBuffer cmd;
  VkImage swapchain_image;
  VkImageView swapchain_view;
  VkExtent2D swapchain_extent;
};

using VkFrameFn = FnRef<void(const VkFrameContext&)>;

fn vk_draw_frame_fn(VkContext_Impl& vk, VkFrameFn func) -> VkMsgExpect<void>;

template<typename Fn>
requires(std::invocable<std::remove_cvref_t<Fn>, VkFrameContext>)
fn vk_draw_frame(VkContext_Impl& vk, Fn&& func) -> VkMsgExpect<void> {
  return vk_draw_frame_fn(vk, VkFrameFn{func});
}

using VkImmediateFn = FnRef<void(VkCommandBuffer)>;

fn vk_submit_immediate_fn(VkContext_Impl& vk, VkImmediateFn func) -> VkExpect<void>;

template<typename Fn>
requires(std::invocable<std::remove_cvref_t<Fn>, VkCommandBuffer>)
fn vk_submit_immediate(VkContext_Impl& vk, Fn&& func) -> VkExpect<void> {
  return vk_submit_immediate_fn(vk, VkImmediateFn{func});
}

} // namespace kappa::render
