#pragma once

#include "./vk_util.hpp"

#include "../../util/ptr.hpp"

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
