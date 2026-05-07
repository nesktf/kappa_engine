#include "./vk_pipeline.hpp"
#include "./vk_util.hpp"

namespace kappa::render {

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

VulkanDescPool::VulkanDescPool(create_t, VkDevice device, VkDescriptorPool pool) :
    _device(device), _pool(pool) {
  ka_assert(_device != VK_NULL_HANDLE);
  ka_assert(_pool != VK_NULL_HANDLE);
}

fn VulkanDescPool::create(VkDevice device, const VulkanDescPoolArgs& args)
  -> VkExpect<VulkanDescPool> {
  const auto& [max_sets, ratios] = args;
  Vec<VkDescriptorPoolSize> pool_sizes;
  pool_sizes.reserve(ratios.size());
  for (const auto& [type, ratio] : ratios) {
    pool_sizes.push_back(VkDescriptorPoolSize{
      .type = type,
      .descriptorCount = static_cast<u32>(ratio * max_sets),
    });
  }
  auto pool_info = vkmk_zero<VkDescriptorPoolCreateInfo>();
  pool_info.flags = 0;
  pool_info.maxSets = max_sets;
  pool_info.poolSizeCount = static_cast<u32>(pool_sizes.size());
  pool_info.pPoolSizes = pool_sizes.data();

  VkDescriptorPool pool;
  if (auto ret = vkCreateDescriptorPool(device, &pool_info, vkalloc, &pool); ret != VK_SUCCESS) {
    return {unexpect, ret};
  }
  return {in_place, create_t(), device, pool};
}

fn VulkanDescPool::add_to_delqueue(VulkanDelQueue& queue) -> void {
  queue.enqueue(_pool, _device);
}

fn VulkanDescPool::alloc_set(VkDescriptorSetLayout layout) -> VkExpect<VkDescriptorSet> {
  auto alloc_info = vkmk_zero<VkDescriptorSetAllocateInfo>();
  alloc_info.descriptorPool = _pool;
  alloc_info.descriptorSetCount = 1;
  alloc_info.pSetLayouts = &layout;

  VkDescriptorSet set;
  if (auto ret = vkAllocateDescriptorSets(_device, &alloc_info, &set); ret != VK_SUCCESS) {
    return {unexpect, ret};
  }
  return {in_place, set};
}

fn VulkanDescPool::clear() -> void {
  vkResetDescriptorPool(_device, _pool, 0);
}

fn vk_create_shader(VkDevice device, Span<const u8> src) -> VkExpect<VkShaderModule> {
  auto info = vkmk_zero<VkShaderModuleCreateInfo>();
  info.codeSize = src.size();
  info.pCode = reinterpret_cast<const u32*>(src.data());
  VkShaderModule shader;
  if (auto ret = vkCreateShaderModule(device, &info, vkalloc, &shader); ret != VK_SUCCESS) {
    return {unexpect, ret};
  }
  return {in_place, shader};
}

} // namespace kappa::render
