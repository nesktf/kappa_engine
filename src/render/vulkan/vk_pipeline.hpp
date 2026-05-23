#pragma once

#include "./vk_util.hpp"

#include "../../util/ptr.hpp"

namespace kappa::render {

struct VkDescPoolRatio {
  VkDescriptorType type;
  f32 ratio;
};

struct VkDescAllocArgs {
  u32 max_sets;
  Span<const VkDescPoolRatio> ratios;
};

class VkDescAlloc {
private:
  struct create_t {};

public:
  VkDescAlloc(create_t, VkDevice device, VkDescriptorPool pool);

public:
  static fn create(VkDevice device, const VkDescAllocArgs& args) -> VkExpect<VkDescAlloc>;

public:
  fn add_to_delqueue(VkDelQueue& queue) -> void;

  fn alloc_sets(VkDescriptorSetLayout layout, VkDescriptorSet* sets, u32 count) -> VkExpect<void>;

  fn clear() -> void;

  fn pool_handle() -> VkDescriptorPool { return _pool; }

private:
  VkDevice _device;
  VkDescriptorPool _pool;
};

fn vk_create_shader(VkContext_Impl& vk, Span<const u8> src) -> VkExpect<VkShaderModule>;
fn vk_destroy_shader(VkContext_Impl& vk, VkShaderModule shader) noexcept -> void;

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
  fn disable_depth_test() -> VkGfxPipelineBuilder&;

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
