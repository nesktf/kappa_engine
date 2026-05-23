#pragma once

#include "./vk_common.hpp"

#include "../../util/function.hpp"
#include "../../util/ptr.hpp"

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
public:
  struct Deleter {
    void operator()(VkContext_Impl* vk) noexcept { VkContext::destroy(vk); }
  };

private:
  struct create_t {};

public:
  VkContext(create_t, VkContext_Impl* ctx) noexcept : _vk(ctx) {}

  static fn create(const VkContextArgs& args) -> VkMsgExpect<VkContext>;
  static fn destroy(VkContext_Impl* vk) noexcept -> void;

public:
  fn device() -> VkDevice;
  fn physical_device() -> VkPhysicalDevice;
  fn allocator() -> VkMemAllocator;

public:
  VkContext_Impl& get() { return *_vk.get(); }

  operator VkContext_Impl&() { return *_vk.get(); }

private:
  std::unique_ptr<VkContext_Impl, Deleter> _vk;
};

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
