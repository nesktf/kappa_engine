#pragma once

#include "./vk_private.hpp"

struct ka_VulkanImage {
  ka_VulkanContext vk;
  VkImage image;
  VkImageView view;
  VmaAllocation alloc;
  VkExtent3D extent;
  VkFormat format;
};

KA_CHECK_OPAQUE(ka_VulkanImage);

struct ka_VulkanBuffer {
  ka_VulkanContext vk;
  VkBuffer buffer;
  VmaAllocation alloc;
  VmaAllocationInfo info;
};

KA_CHECK_OPAQUE(ka_VulkanBuffer);

namespace kappa::render {

fn vk_create_vma_alloc(VkInstance vk, VkDevice device, VkPhysicalDevice physical_device)
  -> VkExpect<VmaAllocator>;

fn vk_alloc_image(ka_VulkanImage& image, VkDevice device, VmaAllocator vma, VkExtent3D extent,
                  VkFormat format) -> VkExpect<void>;
fn vk_dealloc_image(VmaAllocator vma, VkImage image, VmaAllocation alloc) -> void;

fn vk_alloc_buffer(ka_VulkanBuffer& buffer, VmaAllocator vma, usize size, VkBufferUsageFlags usage,
                   VkMemoryPropertyFlags mem_usage) -> VkExpect<void>;
fn vk_dealloc_buffer(VmaAllocator vma, VkBuffer buffer, VmaAllocation alloc) -> void;

} // namespace kappa::render
