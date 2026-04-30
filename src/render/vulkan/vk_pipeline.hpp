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

fn vkpool_create(VkDevice device, u32 max_sets, Span<const VulkanDescPoolRatio> ratios)
  -> VkExpect<VkDescriptorPool>;
fn vkpool_allocate(VkDescriptorPool pool, VkDevice device, VkDescriptorSetLayout layout)
  -> VkExpect<VkDescriptorSet>;
fn vkpool_clear_descriptors(VkDescriptorPool pool, VkDevice device) -> void;

fn vkshader_create(VkDevice device, Span<const u8> source) -> VkExpect<VkShaderModule>;

} // namespace kappa::render
