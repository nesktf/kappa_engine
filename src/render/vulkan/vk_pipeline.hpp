#pragma once

#include "./vk_private.hpp"

#include "../../util/array.hpp"

namespace kappa::render {

class VulkanDescriptorLayoutBuilder {
public:
  VulkanDescriptorLayoutBuilder() = default;

public:
  fn add_binding(u32 binding, VkDescriptorType type) -> void;

  fn clear() -> void;

  fn build(VkDevice device, VkShaderStageFlags stages, void* next = nullptr,
           VkDescriptorSetLayoutCreateFlags flags = 0) -> VkDescriptorSetLayout;

private:
  Vec<VkDescriptorSetLayoutBinding> _bindings;
};

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

  fn alloc_set(VkDescriptorSetLayout layout) -> VkExpect<VkDescriptorSet>;

  fn clear() -> void;

  fn pool_handle() -> VkDescriptorPool { return _pool; }

private:
  VkDevice _device;
  VkDescriptorPool _pool;
};

fn vk_create_shader(VkDevice device, Span<const u8> src) -> VkExpect<VkShaderModule>;

} // namespace kappa::render
