#pragma once

#include "render/vulkan/vk_common.hpp"

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
  struct Self;
  using opaque_type = TypeBuffer<Self, 72, 8>;

public:
  VkAllocBuff(create_t, Self&& data);

public:
  static fn create(VkMemAllocator alloc, const VkBufferArgs& args) -> VkExpect<VkAllocBuff>;

public:
  fn mapped_data() const -> void*;
  fn size() const -> VkDeviceSize;
  fn addr(VkDevice device) const -> VkDeviceAddress;
  fn buffer() const -> VkBuffer;
  fn allocation() const -> VkAllocationMem;

public:
  operator Self&() { return *self; }

  operator const Self&() const { return *self; }

private:
  opaque_type self;
};

fn vk_destroy_buffer(VkMemAllocator alloc, VkAllocBuff::Self& buff) noexcept -> void;

} // namespace kappa::render
