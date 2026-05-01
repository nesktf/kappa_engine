#pragma once

#include "./vk_private.hpp"

#include "../../util/array.hpp"

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

} // namespace kappa::render
