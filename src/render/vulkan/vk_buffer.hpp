#pragma once

#include "./vk_private.hpp"

namespace kappa::render {

fn vk_create_vma_alloc(VkInstance vk, VkDevice device, VkPhysicalDevice physical_device)
  -> VkExpect<VmaAllocator>;

struct VulkanImage {
  VkImage image;
  VkImageView view;
  VmaAllocation alloc;
  VkExtent3D extent;
  VkFormat format;
};

struct VulkanImageArgs {
  VkDevice device;
  VmaAllocator vma;
  VkExtent2D extent;
  VkFormat format;
};

fn vk_alloc_image(const VulkanImageArgs& args) -> VkExpect<VulkanImage>;

} // namespace kappa::render
