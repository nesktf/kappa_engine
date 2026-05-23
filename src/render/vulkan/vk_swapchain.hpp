#pragma once

#include "./vk_util.hpp"

#include "../../util/array.hpp"
#include "../../util/ptr.hpp"

namespace kappa::render {

struct VkSwapchainArgs {
  VkDevice device;
  VkPhysicalDevice physical_device;
  VkSurfaceKHR surface;
  VkExtent2D surface_extent;
  Span<const VkSurfaceFormatKHR> surface_formats;
  Span<const VkPresentModeKHR> surface_present_modes;
  u32 graphics_queue, present_queue;
};

class VkSwapchain {
private:
  struct create_t {};

public:
  VkSwapchain(create_t, VkSwapchainKHR swapchain, VkFormat format, VkExtent2D extent,
              UniqueArray<VkImage>&& images, UniqueArray<VkImageView>&& image_views);

public:
  static fn create(const VkSwapchainArgs& args, VkSwapchainKHR old_swapchain = VK_NULL_HANDLE)
    -> VkExpect<VkSwapchain>;

public:
  fn destroy(VkDevice device) -> void;

public:
  fn swapchain() const -> VkSwapchainKHR { return _swapchain; }

  fn format() const -> VkFormat { return _format; }

  fn extent() const -> VkExtent2D { return _extent; }

  fn images() const -> Span<const VkImage> { return {_images.data(), _images.size()}; }

  fn image_views() const -> Span<const VkImageView> {
    return {_image_views.data(), _image_views.size()};
  }

private:
  VkSwapchainKHR _swapchain;
  VkFormat _format;
  VkExtent2D _extent;
  UniqueArray<VkImage> _images;
  UniqueArray<VkImageView> _image_views;
};

class VkFrameData {
private:
  struct create_t {};

public:
  struct FrameData {
    VkCommandPool cmdpool;
    VkCommandBuffer cmdbuf;
    VkFence render_fen;
    VkSemaphore swapchain_sem, render_sem;
    u32 swapchain_idx;
  };

public:
  VkFrameData(create_t, std::array<FrameData, MAX_FRAMES_IN_FLIGHT>&& frames, VkDevice device);

public:
  static fn create(VkDevice device, u32 graphics_queue) -> VkExpect<VkFrameData>;

public:
  fn add_to_delqueue(VkDelQueue& queue) -> void;
  fn next_frame() -> FrameData&;
  fn curr_frame() -> FrameData&;
  fn frames() -> Span<FrameData, MAX_FRAMES_IN_FLIGHT>;

private:
  std::array<FrameData, MAX_FRAMES_IN_FLIGHT> _frames;
  VkDevice _device;
  u64 _curr_frame;
};

} // namespace kappa::render
