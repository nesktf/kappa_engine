#pragma once

#include "./vk_private.hpp"

#include "./vk_pipeline.hpp"
#include "./vk_util.hpp"

#include "../../util/array.hpp"
#include "../../util/buffer.hpp"

namespace kappa::render {

struct VulkanSwapchainArgs {
  VkDevice device;
  VkPhysicalDevice physical_device;
  VkSurfaceKHR surface;
  VkExtent2D surface_extent;
  Span<const VkSurfaceFormatKHR> surface_formats;
  Span<const VkPresentModeKHR> surface_present_modes;
  u32 graphics_queue, present_queue;
};

class VulkanSwapchain {
private:
  struct create_t {};

public:
  VulkanSwapchain(create_t, VkSwapchainKHR swapchain, VkFormat format, VkExtent2D extent,
                  UniqueArray<VkImage>&& images, UniqueArray<VkImageView>&& image_views);

public:
  static fn create(const VulkanSwapchainArgs& args, VkSwapchainKHR old_swapchain = VK_NULL_HANDLE)
    -> VkExpect<VulkanSwapchain>;

public:
  fn destroy(VkDevice device) -> void;

public:
  VkSwapchainKHR swapchain() const { return _swapchain; }

  VkFormat format() const { return _format; }

  VkExtent2D extent() const { return _extent; }

  Span<const VkImage> images() const { return {_images.data(), _images.size()}; }

  Span<const VkImageView> image_views() const {
    return {_image_views.data(), _image_views.size()};
  }

private:
  VkSwapchainKHR _swapchain;
  VkFormat _format;
  VkExtent2D _extent;
  UniqueArray<VkImage> _images;
  UniqueArray<VkImageView> _image_views;
};

class VulkanFrameData {
private:
  struct create_t {};
  struct TempFrameData;

public:
  struct FrameData {
    VkCommandPool cmdpool;
    VkCommandBuffer cmdbuf;
    VkFence render_fen;
    VkSemaphore swapchain_sem, render_sem;
    VulkanDescPool pool;
    VulkanDelQueue delqueue;
  };

public:
  VulkanFrameData(create_t, TempFrameData* frames, VkDevice device);
  ~VulkanFrameData();
  KA_DO_MOVE(VulkanFrameData);
  KA_NO_COPY(VulkanFrameData);

public:
  static fn create(VkDevice device, u32 graphics_queue, const VulkanDescPoolArgs& descpool_args)
    -> VkExpect<VulkanFrameData>;

public:
  fn add_to_delqueue(VulkanDelQueue& queue) -> void;
  fn next_frame() -> FrameData&;
  fn curr_frame() -> FrameData&;

private:
  AlignedTypeBuffer<FrameData[MAX_FRAMES_IN_FLIGHT]> _frames;
  VkDevice _device;
  u64 _curr_frame;
};

} // namespace kappa::render
