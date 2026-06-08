#define VMA_IMPLEMENTATION
#include "./vk_private.hpp"

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
  KA_VK_UNEX(vmaCreateAllocator(&vma_info, &vmalloc));

  return {in_place, vmalloc};
}

fn vk_alloc_image(VkAllocImage_Impl& image, VkDevice device, VmaAllocator vma, VkExtent3D extent,
                  VkFormat format, VkImageUsageFlags draw_usage) -> VkExpect<void> {
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
  const auto aspect_bit =
    (draw_usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)           ? VK_IMAGE_ASPECT_COLOR_BIT
    : (draw_usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ? VK_IMAGE_ASPECT_DEPTH_BIT
                                                                 : 0;
  const auto rview_info = vkmk_imageview_info(format, image.image, aspect_bit);
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

VkAllocImage::VkAllocImage(create_t, VkAllocImage_Impl&& data) {
  _data.construct(std::move(data));
}

fn VkAllocImage::allocate(VkContext_Impl& vk, const VkImageArgs& args) -> VkExpect<VkAllocImage> {
  const auto device = vk.device.device();
  const auto vmalloc = vk.vmalloc;
  VkAllocImage_Impl image;
  std::memset(&image, 0x00, sizeof(image));
  return kappa::render::vk_alloc_image(image, device, vmalloc, args.extent, args.format,
                                       args.usage)
    .transform([&]() -> VkAllocImage { return {create_t(), std::move(image)}; });
}

fn vk_dealloc_image(VkContext_Impl& vk, VkAllocImage_Impl& image) noexcept -> void {
  if (image.image == VK_NULL_HANDLE) {
    return;
  }
  vkDestroyImageView(vk.device.device(), image.view, vkalloc);
  vk_dealloc_image(vk.vmalloc, image.image, image.alloc);
  image.image = VK_NULL_HANDLE;
}

fn VkAllocImage::extent() const -> VkExtent3D {
  return _data->extent;
}

fn VkAllocImage::format() const -> VkFormat {
  return _data->format;
}

fn VkAllocImage::view() const -> VkImageView {
  return _data->view;
}

fn VkAllocImage::image() const -> VkImage {
  return _data->image;
}

fn vk_alloc_buffer(VkAllocBuff_Impl& buffer, VmaAllocator vma, usize size,
                   VkBufferUsageFlags usage, VmaMemoryUsage mem_usage) -> VkExpect<void> {
  auto buffer_info = vkmk_zero<VkBufferCreateInfo>();
  buffer_info.size = size;
  buffer_info.usage = usage;

  VmaAllocationCreateInfo alloc_info{};
  alloc_info.usage = mem_usage;
  alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

  KA_VK_UNEX(
    vmaCreateBuffer(vma, &buffer_info, &alloc_info, &buffer.buffer, &buffer.alloc, &buffer.info));

  return {};
}

VkAllocBuff::VkAllocBuff(create_t, VkAllocBuff_Impl&& data) {
  _data.construct(std::move(data));
}

fn VkAllocBuff::allocate(VkContext_Impl& vk, const VkBufferArgs& args) -> VkExpect<VkAllocBuff> {
  const auto vmalloc = vk.vmalloc;
  VkAllocBuff_Impl buffer;
  std::memset(&buffer, 0x00, sizeof(buffer));
  return kappa::render::vk_alloc_buffer(buffer, vmalloc, args.size, args.usage,
                                        (VmaMemoryUsage)args.mem_usage)
    .transform([&]() -> VkAllocBuff { return {create_t(), std::move(buffer)}; });
}

fn vk_dealloc_buffer(VmaAllocator vma, VkBuffer buffer, VmaAllocation alloc) -> void {
  vmaDestroyBuffer(vma, buffer, alloc);
}

fn vk_dealloc_buffer(VkContext_Impl& vk, VkAllocBuff_Impl& buff) noexcept -> void {
  if (buff.buffer == VK_NULL_HANDLE) {
    return;
  }
  vk_dealloc_buffer(vk.vmalloc, buff.buffer, buff.alloc);
  buff.buffer = VK_NULL_HANDLE;
}

fn VkAllocBuff::mapped_data() const -> void* {
  return _data->info.pMappedData;
}

fn VkAllocBuff::size() const -> VkDeviceSize {
  return _data->info.size;
}

fn VkAllocBuff::addr(VkDevice device) const -> VkDeviceAddress {
  auto address_info = vkmk_zero<VkBufferDeviceAddressInfo>();
  address_info.buffer = _data->buffer;
  return vkGetBufferDeviceAddress(device, &address_info);
}

fn VkAllocBuff::buffer() const -> VkBuffer {
  return _data->buffer;
}

} // namespace kappa::render
