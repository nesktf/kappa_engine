#pragma once

#include "./vk_private.hpp"

#include "../../util/array.hpp"
#include "../../util/ptr.hpp"

namespace kappa::render {

struct VulkanDescPoolRatio {
  VkDescriptorType type;
  f32 ratio;
};

struct VulkanDescPoolArgs {
  u32 max_sets;
  Span<const VulkanDescPoolRatio> ratios;
};

class VulkanDescPool {
private:
  struct create_t {};

public:
  VulkanDescPool(create_t, VkDevice device, VkDescriptorPool pool);

public:
  static fn create(VkDevice device, const VulkanDescPoolArgs& args) -> VkExpect<VulkanDescPool>;

public:
  fn add_to_delqueue(VulkanDelQueue& queue) -> void;

  fn alloc_sets(VkDescriptorSetLayout layout, VkDescriptorSet* sets, u32 count) -> VkExpect<void>;

  fn clear() -> void;

  fn pool_handle() -> VkDescriptorPool { return _pool; }

private:
  VkDevice _device;
  VkDescriptorPool _pool;
};

} // namespace kappa::render
