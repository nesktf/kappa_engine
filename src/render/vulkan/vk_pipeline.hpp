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

#if 0
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

fn VulkanDescriptorLayoutBuilder::add_binding(u32 binding, VkDescriptorType type) -> void {
  VkDescriptorSetLayoutBinding bind{};
  bind.binding = binding;
  bind.descriptorCount = 1;
  bind.descriptorType = type;
  _bindings.push_back(bind);
}

fn VulkanDescriptorLayoutBuilder::clear() -> void {
  _bindings.clear();
}

fn VulkanDescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags stages, void* next,
                                        VkDescriptorSetLayoutCreateFlags flags)
  -> VkDescriptorSetLayout {
  for (auto& binding : _bindings) {
    binding.stageFlags |= stages;
  }
  auto info = vkmk_zero<VkDescriptorSetLayoutCreateInfo>(next);
  info.pBindings = _bindings.data();
  info.bindingCount = (u32)_bindings.size();
  info.flags = flags;

  VkDescriptorSetLayout set;
  KA_VK_ASSERT(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));
  return set;
}
#endif

} // namespace kappa::render
