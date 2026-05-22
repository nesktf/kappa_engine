#define VMA_IMPLEMENTATION
#include "./vk_buffer.hpp"

#include "./vk_context.hpp"
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
  KA_VK_UNEX(vmaCreateAllocator(&vma_info, &vmalloc));

  return {in_place, vmalloc};
}

fn vk_alloc_image(ka_VkImage& image, VkDevice device, VmaAllocator vma, VkExtent3D extent,
                  VkFormat format) -> VkExpect<void> {
  VkImageUsageFlags draw_usage{};
  draw_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  draw_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  draw_usage |= VK_IMAGE_USAGE_STORAGE_BIT;
  draw_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  const auto rimg_info = vkmk_image_info(format, draw_usage, extent);

  // Allocate the image data
  VmaAllocationCreateInfo rimg_allocinfo{};
  rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  KA_VK_UNEX(vmaCreateImage(vma, &rimg_info, &rimg_allocinfo, &image.image, &image.alloc, nullptr))
  DeferFn image_err = [&]() {
    vk_dealloc_image(vma, image.image, image.alloc);
  };

  // Build image view
  const auto rview_info = vkmk_imageview_info(format, image.image, VK_IMAGE_ASPECT_COLOR_BIT);
  KA_VK_UNEX(vkCreateImageView(device, &rview_info, vkalloc, &image.view));

  image.extent.width = extent.width;
  image.extent.height = extent.height;
  image.extent.depth = extent.depth;
  image.format = format;
  image_err.disengage();
  return {};
}

fn vk_dealloc_image(VmaAllocator vma, VkImage image, VmaAllocation alloc) -> void {
  vmaDestroyImage(vma, image, alloc);
}

} // namespace kappa::render

VkResult ka_vk_alloc_image(ka_VkContext vk, ka_VkImage* image, ka_VkImageArgs const* args) {
  ka_assert(image);
  ka_assert(vk);
  ka_assert(args);

  const auto device = vk->device.device();
  const auto vmalloc = vk->vmalloc;
  std::memset(image, 0x00, sizeof(*image));
  return kappa::render::vk_alloc_image(*image, device, vmalloc, args->extent, args->format)
    .error_or(kappa::render::VkError(VK_SUCCESS))
    .code();
}

void ka_vk_dealloc_image(ka_VkContext vk, ka_VkImage* image) {
  if (!vk || !image) {
    return;
  }
  vkDestroyImageView(vk->device.device(), image->view, vkalloc);
  kappa::render::vk_dealloc_image(vk->vmalloc, image->image, image->alloc);
}

void ka_vk_image_get_extent(ka_VkImage const* image, VkExtent3D* extent) {
  (*extent) = image->extent;
}

void ka_vk_image_get_format(ka_VkImage const* image, VkFormat* format) {
  (*format) = image->format;
}

void ka_vk_image_get_target(ka_VkImage const* image, ka_VkTarget* target) {
  (*target) = (ka_VkTarget)image;
}

void ka_vk_image_get_view(ka_VkImage const* image, VkImageView* view) {
  (*view) = image->view;
}

void ka_vk_image_get_handle(ka_VkImage const* image, VkImage* handle) {
  (*handle) = image->image;
}

namespace kappa::render {

fn vk_alloc_buffer(ka_VkBuffer& buffer, VmaAllocator vma, usize size, VkBufferUsageFlags usage,
                   VkMemoryPropertyFlags mem_usage) -> VkExpect<void> {
  auto buffer_info = vkmk_zero<VkBufferCreateInfo>();
  buffer_info.size = size;
  buffer_info.usage = usage;

  VmaAllocationCreateInfo alloc_info{};
  alloc_info.usage = (VmaMemoryUsage)mem_usage;
  alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

  KA_VK_UNEX(
    vmaCreateBuffer(vma, &buffer_info, &alloc_info, &buffer.buffer, &buffer.alloc, &buffer.info));

  return {};
}

fn vk_dealloc_buffer(VmaAllocator vma, VkBuffer buffer, VmaAllocation alloc) -> void {
  vmaDestroyBuffer(vma, buffer, alloc);
}

} // namespace kappa::render

VkResult ka_vk_alloc_buffer(ka_VkContext vk, ka_VkBuffer* buffer, ka_VkBufferArgs const* args) {
  ka_assert(vk);
  ka_assert(buffer);
  ka_assert(args);

  const auto vmalloc = vk->vmalloc;
  std::memset(buffer, 0x00, sizeof(*buffer));
  return kappa::render::vk_alloc_buffer(*buffer, vmalloc, args->size, args->usage, args->mem_usage)
    .error_or(kappa::render::VkError(VK_SUCCESS))
    .code();
}

void ka_vk_dealloc_buffer(ka_VkContext vk, ka_VkBuffer* buffer) {
  if (!buffer) {
    return;
  }
  kappa::render::vk_dealloc_buffer(vk->vmalloc, buffer->buffer, buffer->alloc);
}

void ka_vk_buffer_get_mapped_data(ka_VkBuffer const* buffer, void** data) {
  (*data) = buffer->info.pMappedData;
}

void ka_vk_buffer_get_size(ka_VkBuffer const* buffer, size_t* size) {
  (*size) = (size_t)buffer->info.size;
}

void ka_vk_buffer_get_addr(ka_VkBuffer const* buffer, ka_VkContext vk, VkDeviceAddress* addr) {
  const auto device = vk->device.device();
  auto address_info = kappa::render::vkmk_zero<VkBufferDeviceAddressInfo>();
  address_info.buffer = buffer->buffer;
  (*addr) = vkGetBufferDeviceAddress(device, &address_info);
}

void ka_vk_buffer_get_handle(ka_VkBuffer const* buffer, VkBuffer* handle) {
  (*handle) = buffer->buffer;
}

void ka_vk_buffer_transfer(ka_VkContext vk, ka_VkBuffer const* src, VkDeviceSize src_offset,
                           VkDeviceSize src_size, ka_VkBuffer* dst, VkDeviceSize dst_offset,
                           VkDeviceSize dst_size) {}
