#include "./vk_pipeline.hpp"
#include "./vk_private.hpp"
#include <vulkan/vulkan_core.h>

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
  VkDescriptorSetLayoutCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  info.pNext = next;

  info.pBindings = _bindings.data();
  info.bindingCount = (u32)_bindings.size();
  info.flags = flags;

  VkDescriptorSetLayout set;
  VK_ASSERT(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));
  return set;
}

fn vkpool_create(VkDevice device, u32 max_sets, Span<const VulkanDescPoolRatio> ratios)
  -> VkExpect<VkDescriptorPool> {
  Vec<VkDescriptorPoolSize> pool_sizes;
  pool_sizes.reserve(ratios.size());
  for (const auto& [type, ratio] : ratios) {
    pool_sizes.push_back(VkDescriptorPoolSize{
      .type = type,
      .descriptorCount = static_cast<u32>(ratio * max_sets),
    });
  }
  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = 0;
  pool_info.maxSets = max_sets;
  pool_info.poolSizeCount = static_cast<u32>(pool_sizes.size());
  pool_info.pPoolSizes = pool_sizes.data();

  VkDescriptorPool pool;
  if (auto ret = vkCreateDescriptorPool(device, &pool_info, vkalloc, &pool); ret != VK_SUCCESS) {
    return {unexpect, ret};
  }
  return {in_place, pool};
}

fn vkpool_allocate(VkDescriptorPool pool, VkDevice device, VkDescriptorSetLayout layout)
  -> VkExpect<VkDescriptorSet> {
  VkDescriptorSetAllocateInfo alloc_info{};
  alloc_info.pNext = nullptr;
  alloc_info.descriptorPool = pool;
  alloc_info.descriptorSetCount = 1;
  alloc_info.pSetLayouts = &layout;

  VkDescriptorSet set;
  if (auto ret = vkAllocateDescriptorSets(device, &alloc_info, &set); ret != VK_SUCCESS) {
    return {unexpect, ret};
  }
  return set;
}

fn vkpool_clear_descriptors(VkDescriptorPool pool, VkDevice device) -> void {
  vkResetDescriptorPool(device, pool, 0);
}

fn vkshader_create(VkDevice device, Span<const u8> source) -> VkExpect<VkShaderModule> {
  VkShaderModuleCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  info.pNext = nullptr;

  info.codeSize = source.size();
  info.pCode = reinterpret_cast<const u32*>(source.data());
  VkShaderModule shader;
  if (auto ret = vkCreateShaderModule(device, &info, vkalloc, &shader); ret != VK_SUCCESS) {
    return {unexpect, ret};
  }
  return {in_place, shader};
}

} // namespace kappa::render
