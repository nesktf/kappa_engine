#pragma once

#include "./vk_private.hpp"

namespace kappa::render {

struct VkAllocImage_Impl {
  VkImage image;
  VkImageView view;
  VmaAllocation alloc;
  VkExtent3D extent;
  VkFormat format;
};

static_assert(VkAllocImage::opaque_type::check_params(), "Invalid VkAllocImage opaque params");

fn vk_create_vma_alloc(VkInstance vk, VkDevice device, VkPhysicalDevice physical_device)
  -> VkExpect<VmaAllocator>;

fn vk_alloc_image(VkAllocImage_Impl& image, VkDevice device, VmaAllocator vma, VkExtent3D extent,
                  VkFormat format) -> VkExpect<void>;
fn vk_dealloc_image(VmaAllocator vma, VkImage image, VmaAllocation alloc) -> void;

struct VkAllocBuff_Impl {
  VkBuffer buffer;
  VmaAllocation alloc;
  VmaAllocationInfo info;
};

static_assert(VkAllocBuff::opaque_type::check_params(), "Invalid VkAllocBuff opaque params");

fn vk_alloc_buffer(VkAllocBuff_Impl& buffer, VmaAllocator vma, usize size,
                   VkBufferUsageFlags usage, VkMemoryPropertyFlags mem_usage) -> VkExpect<void>;
fn vk_dealloc_buffer(VmaAllocator vma, VkBuffer buffer, VmaAllocation alloc) -> void;

} // namespace kappa::render
