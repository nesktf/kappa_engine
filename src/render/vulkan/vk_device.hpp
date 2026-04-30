#pragma once

#include "./vk_private.hpp"

#include "../../util/array.hpp"

namespace kappa::render {

struct VulkanDevice {
  VkDevice device;
  VkPhysicalDevice physical_device;
  u32 graphics_queue;
  u32 present_queue;
  u32 transfer_queue;
  Vec<VkSurfaceFormatKHR> surface_formats;
  Vec<VkPresentModeKHR> surface_present_modes;
};

fn vk_create_device(VkInstance vk, VkSurfaceKHR surface) -> VkExpect<VulkanDevice>;

} // namespace kappa::render
