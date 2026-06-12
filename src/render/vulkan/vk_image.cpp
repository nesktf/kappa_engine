#include "./vk_image.hpp"

#include "./vk_private.hpp"

namespace kappa::render {

struct VkAllocImage::Self {
  VkImage image;
  VkImageView view;
  VmaAllocation alloc;
  VkExtent3D extent;
  VkFormat format;
};

static_assert(VkAllocImage::opaque_type::check_params(), "Invalid VkAllocImage opaque params");

VkAllocImage::VkAllocImage(create_t, Self&& data) {
  self.construct(std::move(data));
}

namespace {

fn dealloc_image_defer(VmaAllocator vma, VkImage image, VmaAllocation alloc) -> fn {
  return [=]() {
    vmaDestroyImage(vma, image, alloc);
  };
}

fn calc_mipmap_levels(u32 width, u32 height) -> u32 {
  return (u32)(std::floor(std::log2(std::max(width, height)))) + 1;
}

} // namespace

fn VkAllocImage::create(VkDevice device, VkMemAllocator allocator, const VkImageArgs& args)
  -> VkExpect<VkAllocImage> {
  const auto& [extent, format, usage, mipmaps] = args;
  auto rimg_info = vkmk_image_info(format, usage, extent);
  if (args.mipmaps == KA_VK_ENABLE_MIPMAPS) {
    rimg_info.mipLevels = calc_mipmap_levels(extent.width, extent.height);
  }
  VmaAllocator vma = (VmaAllocator)allocator;

  // Allocate the image data
  VmaAllocationCreateInfo rimg_allocinfo{};
  rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  Self self;
  std::memset(&self, 0x00, sizeof(self));
  KA_VK_UNEX(vmaCreateImage(vma, &rimg_info, &rimg_allocinfo, &self.image, &self.alloc, nullptr))
  DeferFn image_err = dealloc_image_defer(vma, self.image, self.alloc);

  VkImageAspectFlags aspect_flags = 0;
  if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT || usage & VK_IMAGE_USAGE_SAMPLED_BIT) {
    aspect_flags |= VK_IMAGE_ASPECT_COLOR_BIT;
  } else if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
    aspect_flags |= VK_IMAGE_ASPECT_DEPTH_BIT;
  }

  const auto rview_info = vkmk_imageview_info(format, self.image, aspect_flags);
  KA_VK_UNEX(vkCreateImageView(device, &rview_info, vkalloc, &self.view));
  self.extent = extent;
  self.format = format;
  image_err.disengage();
  return {in_place, create_t(), std::move(self)};
}

fn vk_destroy_image(VkDevice device, VkMemAllocator alloc, VkAllocImage::Self& imag) noexcept
  -> void {
  if (!device || !alloc) {
    return;
  }
  vkDestroyImageView(device, imag.view, vkalloc);
  dealloc_image_defer((VmaAllocator)alloc, imag.image, imag.alloc)();
}

fn VkAllocImage::extent() const -> VkExtent3D {
  return self->extent;
}

fn VkAllocImage::format() const -> VkFormat {
  return self->format;
}

fn VkAllocImage::view() const -> VkImageView {
  return self->view;
}

fn VkAllocImage::image() const -> VkImage {
  return self->image;
}

fn VkAllocImage::allocation() const -> VkAllocationMem {
  return (VkAllocationMem)self->alloc;
}

fn vk_create_sampler(VkDevice device, VkFilter mag, VkFilter min) -> VkExpect<VkSampler> {
  auto sampl = vkmk_zero<VkSamplerCreateInfo>();
  sampl.magFilter = mag;
  sampl.minFilter = min;
  VkSampler sampler;
  KA_VK_UNEX(vkCreateSampler(device, &sampl, vkalloc, &sampler))
  return {in_place, sampler};
}

} // namespace kappa::render
