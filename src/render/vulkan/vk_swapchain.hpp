#pragma once

#include "./vk_private.hpp"

#include "../../util/array.hpp"

namespace kappa::render {

struct VulkanSwapchain {
  VkSwapchainKHR swapchain;
  VkFormat format;
  UniqueArray<VkImage> images;
  UniqueArray<VkImageView> image_views;
  VkExtent2D extent;
};

struct VulkanSwapchainArgs {
  VkDevice device;
  VkPhysicalDevice physical_device;
  VkSurfaceKHR surface;
  VkExtent2D surface_extent;
  Span<VkSurfaceFormatKHR> surface_formats;
  Span<VkPresentModeKHR> surface_present_modes;
  u32 graphics_queue, present_queue;
};

fn vk_create_swapchain(const VulkanSwapchainArgs& args,
                       VkSwapchainKHR old_swapchain = VK_NULL_HANDLE) -> VkExpect<VulkanSwapchain>;

fn vk_destroy_swapchain(VkDevice device, VulkanSwapchain& swapchain) -> void;

} // namespace kappa::render
