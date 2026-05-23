#pragma once

#include "./vk_common.hpp"

#include "../../util/buffer.hpp"

namespace kappa::render {

// Same as VmaMemoryUsage
enum VkMemoryUsage {
  KA_VK_MEM_USAGE_GPU_ONLY = 1,
  KA_VK_MEM_USAGE_CPU_ONLY = 2,
  KA_VK_MEM_USAGE_CPU_TO_GPU = 3,
  KA_VK_MEM_USAGE_GPU_TO_CPU = 4,
  KA_VK_MEM_USAGE_CPU_COPY = 5,
};

struct VkBufferArgs {
  size_t size;
  VkBufferUsageFlags usage;
  VkMemoryUsage mem_usage;
};

class VkAllocBuff {
private:
  struct create_t {};

public:
  using opaque_type = TypeBuffer<VkAllocBuff_Impl, 72, 8>;

public:
  VkAllocBuff(create_t, VkAllocBuff_Impl&& data);

public:
  static fn allocate(VkContext_Impl& ctx, const VkBufferArgs& args) -> VkExpect<VkAllocBuff>;

public:
  fn mapped_data() -> void*;
  fn size() -> VkDeviceSize;
  fn addr(VkContext_Impl& vk) -> VkDeviceAddress;
  fn buffer() -> VkBuffer;

public:
  operator VkAllocBuff_Impl&() { return *_data.get(); }

  operator const VkAllocBuff_Impl&() const { return *_data.get(); }

private:
  opaque_type _data;
};

fn vk_dealloc_buffer(VkContext_Impl& vk, VkAllocBuff_Impl& buff) noexcept -> void;

struct VkImageArgs {
  VkExtent3D extent;
  VkFormat format;
};

class VkAllocImage {
private:
  struct create_t {};

public:
  using opaque_type = TypeBuffer<VkAllocImage_Impl, 40, 8>;

public:
  VkAllocImage(create_t, VkAllocImage_Impl&& data);

public:
  static fn allocate(VkContext_Impl& ctx, const VkImageArgs& args) -> VkExpect<VkAllocImage>;

public:
  fn extent() const -> VkExtent3D;
  fn format() const -> VkFormat;
  fn view() const -> VkImageView;
  fn image() const -> VkImage;

public:
  operator VkAllocImage_Impl&() { return *_data.get(); }

  operator const VkAllocImage_Impl&() const { return *_data.get(); }

private:
  opaque_type _data;
};

fn vk_dealloc_image(VkContext_Impl& vk, VkAllocImage_Impl& image) noexcept -> void;

} // namespace kappa::render
