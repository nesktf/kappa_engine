#define VMA_IMPLEMENTATION
#include "./vk_buffer.hpp"
#include "./vk_util.hpp"

namespace kappa::render {

fn vk_create_vma_alloc(VkInstance vk, VkDevice device, VkPhysicalDevice physical_device)
  -> VkExpect<VmaAllocator> {
  VmaAllocatorCreateInfo vma_info{};
  vma_info.instance = vk;
  vma_info.device = device;
  vma_info.physicalDevice = physical_device;
  vma_info.vulkanApiVersion = KA_VULKAN_VERSION;
  vma_info.flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT |
                   VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

  VmaAllocator vmalloc;
  if (auto ret = vmaCreateAllocator(&vma_info, &vmalloc); ret != VK_SUCCESS) {
    return {unexpect, ret};
  }
  return {in_place, vmalloc};
}

fn vk_alloc_image(const VulkanImageArgs& args) -> VkExpect<VulkanImage> {
  VulkanImage image{};
  image.extent.width = args.extent.width;
  image.extent.height = args.extent.height;
  image.extent.depth = 1;
  image.format = args.format;

  VkImageUsageFlags draw_usage{};
  draw_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  draw_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  draw_usage |= VK_IMAGE_USAGE_STORAGE_BIT;
  draw_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  const auto rimg_info = vkmk_image_info(args.format, draw_usage, image.extent);

  // Allocate the image data
  VmaAllocationCreateInfo rimg_allocinfo{};
  rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (auto ret =
        vmaCreateImage(args.vma, &rimg_info, &rimg_allocinfo, &image.image, &image.alloc, nullptr);
      ret != VK_SUCCESS) {
    return {unexpect, ret};
  }

  // Build image view
  const auto rview_info =
    vkmk_imageview_info(image.format, image.image, VK_IMAGE_ASPECT_COLOR_BIT);
  if (auto ret = vkCreateImageView(args.device, &rview_info, vkalloc, &image.view);
      ret != VK_SUCCESS) {
    vmaDestroyImage(args.vma, image.image, image.alloc);
    return {unexpect, ret};
  }
  return {in_place, std::move(image)};
}

fn vk_dealloc_image(VmaAllocator vma, VkImage image, VmaAllocation alloc) -> void {
  vmaDestroyImage(vma, image, alloc);
}

fn vk_alloc_buffer(const VulkanBufferArgs& args) -> VkExpect<VulkanBuffer> {
  auto buffer_info = vkmk_zero<VkBufferCreateInfo>();
  buffer_info.size = args.size;
  buffer_info.usage = args.usage;

  VmaAllocationCreateInfo alloc_info{};
  alloc_info.usage = args.mem_usage;
  alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

  VulkanBuffer out{};
  KA_VK_UNEX(
    vmaCreateBuffer(args.vma, &buffer_info, &alloc_info, &out.buffer, &out.alloc, &out.info));

  return {in_place, std::move(out)};
}

fn vk_dealloc_buffer(VmaAllocator vma, VkBuffer buffer, VmaAllocation alloc) -> void {
  vmaDestroyBuffer(vma, buffer, alloc);
}

} // namespace kappa::render
