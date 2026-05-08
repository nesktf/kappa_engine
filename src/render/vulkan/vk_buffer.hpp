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
fn vk_dealloc_image(VmaAllocator vma, VkImage image, VmaAllocation alloc) -> void;

struct VulkanBuffer {
  VkBuffer buffer;
  VmaAllocation alloc;
  VmaAllocationInfo info;
};

struct VulkanBufferArgs {
  VmaAllocator vma;
  usize size;
  VkBufferUsageFlags usage;
  VmaMemoryUsage mem_usage;
};

fn vk_alloc_buffer(const VulkanBufferArgs& args) -> VkExpect<VulkanBuffer>;
fn vk_dealloc_buffer(VmaAllocator vma, VkBuffer buffer, VmaAllocation alloc) -> void;

} // namespace kappa::render
