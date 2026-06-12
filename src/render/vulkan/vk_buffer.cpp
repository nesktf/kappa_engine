#include "./vk_buffer.hpp"
#include "./vk_util.hpp"

#include "./vk_private.hpp"

namespace kappa::render {

struct VkAllocBuff::Self {
  VkBuffer buffer;
  VmaAllocation alloc;
  VmaAllocationInfo info;
};

static_assert(VkAllocBuff::opaque_type::check_params(), "Invalid VkAllocBuff opaque params");

VkAllocBuff::VkAllocBuff(create_t, Self&& data) {
  self.construct(std::move(data));
}

fn VkAllocBuff::create(VkMemAllocator allocator, const VkBufferArgs& args)
  -> VkExpect<VkAllocBuff> {
  const auto& [size, usage, mem_usage] = args;
  auto buffer_info = vkmk_zero<VkBufferCreateInfo>();
  buffer_info.size = size;
  buffer_info.usage = usage;

  VmaAllocator vma = (VmaAllocator)allocator;
  VmaAllocationCreateInfo alloc_info{};
  alloc_info.usage = (VmaMemoryUsage)mem_usage;
  alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

  Self self;
  std::memset(&self, 0x00, sizeof(self));

  KA_VK_UNEX(
    vmaCreateBuffer(vma, &buffer_info, &alloc_info, &self.buffer, &self.alloc, &self.info));
  return {in_place, create_t(), std::move(self)};
}

fn vk_destroy_buffer(VkMemAllocator allocator, VkAllocBuff::Self& buff) noexcept -> void {
  if (!allocator) {
    return;
  }
  vmaDestroyBuffer((VmaAllocator)allocator, buff.buffer, buff.alloc);
}

fn VkAllocBuff::mapped_data() const -> void* {
  return self->info.pMappedData;
}

fn VkAllocBuff::size() const -> VkDeviceSize {
  return self->info.size;
}

fn VkAllocBuff::addr(VkDevice device) const -> VkDeviceAddress {
  auto address_info = vkmk_zero<VkBufferDeviceAddressInfo>();
  address_info.buffer = self->buffer;
  return vkGetBufferDeviceAddress(device, &address_info);
}

fn VkAllocBuff::buffer() const -> VkBuffer {
  return self->buffer;
}

fn VkAllocBuff::allocation() const -> VkAllocationMem {
  return (VkAllocationMem)self->alloc;
}

} // namespace kappa::render
