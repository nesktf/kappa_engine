#pragma once

#include "./vk_private.hpp"

namespace kappa::render {

fn vk_create_vma_alloc(VkInstance vk, VkDevice device, VkPhysicalDevice physical_device)
  -> VkExpect<VmaAllocator>;

struct VulkanImage_data {
  VkImage image;
  VkImageView view;
  VmaAllocation alloc;
  VkExtent3D extent;
  VkFormat format;
};

KA_CHECK_OPAQUE(VulkanImage_data);

fn vk_alloc_image(VkDevice device, VmaAllocator vma, VkExtent3D extent, VkFormat format)
  -> VkExpect<VulkanImage_data>;
fn vk_dealloc_image(VmaAllocator vma, VkImage image, VmaAllocation alloc) -> void;

struct VulkanBuffer_data {
  VkBuffer buffer;
  VmaAllocation alloc;
  VmaAllocationInfo info;
};

KA_CHECK_OPAQUE(VulkanBuffer_data);

fn vk_alloc_buffer(VmaAllocator vma, usize size, VkBufferUsageFlags usage,
                   VkMemoryPropertyFlags mem_usage) -> VkExpect<VulkanBuffer_data>;
fn vk_dealloc_buffer(VmaAllocator vma, VkBuffer buffer, VmaAllocation alloc) -> void;

} // namespace kappa::render
