#pragma once

#include "render/vulkan/vk_common.hpp"

namespace kappa::render {

enum VkImageMipsFlag {
  KA_VK_DISABLE_MIPMAPS = 0,
  KA_VK_ENABLE_MIPMAPS = 1,
};

struct VkImageArgs {
  VkExtent3D extent;
  VkFormat format;
  VkImageUsageFlags usage;
  VkImageMipsFlag mipmaps;
};

class VkAllocImage {
private:
  struct create_t {};

public:
  struct Self;
  using opaque_type = TypeBuffer<Self, 40, 8>;

public:
  VkAllocImage(create_t, Self&& data);

public:
  static fn create(VkDevice device, VkMemAllocator alloc, const VkImageArgs& args)
    -> VkExpect<VkAllocImage>;

public:
  fn extent() const -> VkExtent3D;
  fn format() const -> VkFormat;
  fn view() const -> VkImageView;
  fn image() const -> VkImage;
  fn allocation() const -> VkAllocationMem;

public:
  operator Self&() { return *self; }

  operator const Self&() const { return *self; }

private:
  opaque_type self;
};

fn vk_destroy_image(VkDevice device, VkMemAllocator alloc, VkAllocImage::Self& imag) noexcept
  -> void;
fn vk_create_sampler(VkDevice device, VkFilter mag, VkFilter min) -> VkExpect<VkSampler>;

} // namespace kappa::render
