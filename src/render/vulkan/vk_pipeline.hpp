#pragma once

#include "./vk_util.hpp"

#include "../../util/ptr.hpp"
#include <vulkan/vulkan_core.h>

namespace kappa::render {

class VkDescLayoutBuilder {
public:
  VkDescLayoutBuilder();

public:
  fn build(VkContext_Impl& vk, VkShaderStageFlags stages, void* pNext = nullptr,
           VkDescriptorSetLayoutCreateFlags flags = 0) -> VkExpect<VkDescriptorSetLayout>;
  fn clear() -> void;

public:
  fn add_binding(u32 binding, VkDescriptorType type) -> VkDescLayoutBuilder&;

private:
  Vec<VkDescriptorSetLayoutBinding> _bindings;
};

fn vk_destroy_desc_layout(VkContext_Impl& vk, VkDescriptorSetLayout layout) noexcept -> void;

struct VkDescPoolRatio {
  VkDescriptorType type;
  f32 ratio;
};

class VkDescAlloc {
public:
  struct Self {
    VkDevice device;
    VkDescriptorPool pool;
  };

  KA_SELF_FORWARD(VkDescAlloc);

public:
  static fn create(VkDevice device, u32 max_sets, Span<const VkDescPoolRatio> ratios)
    -> VkExpect<VkDescAlloc>;

public:
  fn allocate(VkDescriptorSetLayout layout) -> VkExpect<VkDescriptorSet>;
  fn clear() -> void;
  fn destroy() -> void;
};

class VkDynDescAlloc {
public:
  struct Self {
    Vec<VkDescPoolRatio> ratios;
    Vec<VkDescriptorPool> avail;
    Vec<VkDescriptorPool> full;
    VkDevice device;
    u32 sets_per_pool;
  };

  KA_SELF_FORWARD(VkDynDescAlloc);

public:
  static fn create(VkDevice device, u32 initial_sets, Span<const VkDescPoolRatio> ratios)
    -> VkExpect<VkDynDescAlloc>;

public:
  fn allocate(VkDescriptorSetLayout layout, void* pNext = nullptr) -> VkExpect<VkDescriptorSet>;
  fn clear() -> void;
  fn destroy() -> void;
};

enum VkDescImageType {
  // Needs sampler
  KA_VK_DESC_IMAG_TYPE_SAMPLER = VK_DESCRIPTOR_TYPE_SAMPLER,

  // Needs image view and layout (accessed with different samplers in the shader)
  KA_VK_DESC_IMAG_TYPE_SAMPLED_IMAGE = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,

  // Needs image view, image layout and sampler
  KA_VK_DESC_IMAGE_TYPE_COMBINED_IMAGE_SAMPLER = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,

  // Needs image view and layout
  KA_VK_DESC_IMAGE_TYPE_STORAGE_IMAGE = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
};

enum VkDescBuffType {
  KA_VK_DESC_BUFF_TYPE_UNIFORM = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
  KA_VK_DESC_BUFF_TYPE_STORAGE = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
  KA_VK_DESC_BUFF_TYPE_UNIFORM_DYN = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
  KA_VK_DESC_BUFF_TYPE_STORAGE_DYN = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
};

class VkDescWriter {
public:
  VkDescWriter(VkDevice device);

public:
  fn write_buffer(s32 binding, VkBuffer buffer, usize size, usize offset, VkDescBuffType type)
    -> void;

  fn write_image(s32 binding, VkImageView view, VkImageLayout layout, VkSampler sampler,
                 VkDescImageType type) -> void;

  fn write_sampler(s32 binding, VkSampler sampler) -> void {
    write_image(binding, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED, sampler,
                KA_VK_DESC_IMAG_TYPE_SAMPLER);
  }

  fn write_sampled_image(s32 binding, VkImageView view, VkImageLayout layout) -> void {
    write_image(binding, view, layout, VK_NULL_HANDLE, KA_VK_DESC_IMAG_TYPE_SAMPLED_IMAGE);
  }

  fn write_combined_image(s32 binding, VkImageView view, VkImageLayout layout, VkSampler sampler)
    -> void {
    write_image(binding, view, layout, sampler, KA_VK_DESC_IMAGE_TYPE_COMBINED_IMAGE_SAMPLER);
  }

  fn write_storage_image(s32 binding, VkImageView view, VkImageLayout layout) -> void {
    write_image(binding, view, layout, VK_NULL_HANDLE, KA_VK_DESC_IMAGE_TYPE_STORAGE_IMAGE);
  }

public:
  fn clear() -> void;
  fn update_set(VkDescriptorSet set) -> void;

private:
  VkDevice _device;
  Deque<VkDescriptorImageInfo> _image_infos;
  Deque<VkDescriptorBufferInfo> _buff_infos;
  Vec<VkWriteDescriptorSet> _writes;
};

fn vk_create_shader(VkContext_Impl& vk, Span<const u8> src) -> VkExpect<VkShaderModule>;
fn vk_destroy_shader(VkContext_Impl& vk, VkShaderModule shader) noexcept -> void;

class VkPipelineLayoutBuilder {
public:
  VkPipelineLayoutBuilder();

public:
  fn build(VkContext_Impl& vk, VkPipelineLayoutCreateFlags flags = 0)
    -> VkExpect<VkPipelineLayout>;
  fn clear() -> void;

public:
  fn add_push_range(VkShaderStageFlags flags, u32 size, u32 offset) -> VkPipelineLayoutBuilder&;
  fn add_layout(VkDescriptorSetLayout layout) -> VkPipelineLayoutBuilder&;

private:
  Vec<VkDescriptorSetLayout> _set_layouts;
  Vec<VkPushConstantRange> _con_range;
};

fn vk_destroy_pipeline_layout(VkContext_Impl& vk, VkPipelineLayout layout) noexcept -> void;

enum VkDepthWriteEnable {
  KA_VK_DEPTH_WRITE_DISABLE = 0,
  KA_VK_DEPTH_WRITE_ENABLE,
};

class VkGfxPipelineBuilder {
public:
  struct ShaderEntry {
    VkShaderModule shader;
    const char* entrypoint;
  };

  enum ShaderStages {
    STAGE_VERTEX = 0,
    STAGE_FRAGMENT,
    STAGE_GEOMETRY,
    STAGE_TESS_EVAL,
    STAGE_TESS_CTRL,
    STAGE_COUNT,
  };

public:
  VkGfxPipelineBuilder();

public:
  fn build(VkContext_Impl& ctx) -> VkExpect<VkPipeline>;
  fn clear() -> void;

public:
  fn set_layout(VkPipelineLayout layout) -> VkGfxPipelineBuilder&;
  fn add_module(VkShaderStageFlagBits stage, VkShaderModule shader,
                const char* entrypoint = nullptr) -> VkGfxPipelineBuilder&;
  fn set_topology(VkPrimitiveTopology topology) -> VkGfxPipelineBuilder&;
  fn set_poly_mode(VkPolygonMode mode, f32 width = 1.f) -> VkGfxPipelineBuilder&;
  fn set_cull_mode(VkCullModeFlags mode, VkFrontFace front_face) -> VkGfxPipelineBuilder&;
  fn set_color_format(VkFormat format) -> VkGfxPipelineBuilder&;
  fn set_depth_format(VkFormat format) -> VkGfxPipelineBuilder&;
  fn disable_multisampling() -> VkGfxPipelineBuilder&;

  fn disable_blending() -> VkGfxPipelineBuilder&;
  fn enable_blending_additive() -> VkGfxPipelineBuilder&;
  fn enable_blending_alphablend() -> VkGfxPipelineBuilder&;

  fn disable_depth_test() -> VkGfxPipelineBuilder&;
  fn enable_depth_test(VkDepthWriteEnable enable_write, VkCompareOp op) -> VkGfxPipelineBuilder&;

private:
  std::array<ShaderEntry, STAGE_COUNT> _shader_stages;
  VkPipelineInputAssemblyStateCreateInfo _input_assembly;
  VkPipelineRasterizationStateCreateInfo _rasterizer;
  VkPipelineColorBlendAttachmentState _blend_attachment;
  VkPipelineMultisampleStateCreateInfo _multisampling;
  VkPipelineLayout _layout;
  VkPipelineDepthStencilStateCreateInfo _depth_stencil;
  VkPipelineRenderingCreateInfo _rendering_info;
  VkFormat _color_format;
};

fn vk_create_compute_pipeline(VkContext_Impl& vk, VkPipelineLayout layout, VkShaderModule shader,
                              const char* entrypoint = nullptr) -> VkExpect<VkPipeline>;
fn vk_destroy_pipeline(VkContext_Impl& vk, VkPipeline pip) noexcept -> void;

} // namespace kappa::render
