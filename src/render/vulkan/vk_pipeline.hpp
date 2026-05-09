#pragma once

#include "./vk_private.hpp"

#include "../../util/array.hpp"
#include "../../util/ptr.hpp"

struct ka_VulkanPipelineBuilder_impl {
  kappa::Vec<VkPipelineShaderStageCreateInfo> shader_stages;
  VkPipelineInputAssemblyStateCreateInfo input_assembly;
  VkPipelineRasterizationStateCreateInfo rasterizer;
  VkPipelineColorBlendAttachmentState blend_attachment;
  VkPipelineMultisampleStateCreateInfo multisampling;
  VkPipelineLayout layout;
  VkPipelineDepthStencilStateCreateInfo depth_stencil;
  VkPipelineRenderingCreateInfo rendering_info;
  VkFormat color_format;
};

struct ka_VulkanPipeline {
  VkPipeline pipeline;
  VkPipelineLayout layout;
  ka_VulkanContext vk;
};

KA_CHECK_OPAQUE(ka_VulkanPipeline);

struct ka_VulkanShaderModule {
  VkShaderModule shader;
  VkShaderStageFlagBits stage;
  ka_VulkanContext vk;
};

KA_CHECK_OPAQUE(ka_VulkanShaderModule);

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

fn vk_create_compute_pipeline(VkDevice device, VkPipelineLayout layout, Span<const u8> src)
  -> VkExpect<VkPipeline>;

} // namespace kappa::render
