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

VulkanPipelineBuilder::VulkanPipelineBuilder() {
  clear();
}

fn VulkanPipelineBuilder::clear() -> void {
  _input_assembly = vkmk_zero<decltype(_input_assembly)>();
  _rasterizer = vkmk_zero<decltype(_rasterizer)>();
  std::memset(&_blend_attachment, 0x00, sizeof(_blend_attachment));
  _multisampling = vkmk_zero<decltype(_multisampling)>();
  _layout = VK_NULL_HANDLE;
  _depth_stencil = vkmk_zero<decltype(_depth_stencil)>();
  _rendering_info = vkmk_zero<decltype(_rendering_info)>();
  _shader_stages.clear();
}

fn VulkanPipelineBuilder::build(VkDevice device) -> VkExpect<VkPipeline> {
  // Dynamic viewport & scissor
  static constexpr auto dynamic_state =
    std::to_array<VkDynamicState>({VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});
  auto viewport_state = vkmk_zero<VkPipelineViewportStateCreateInfo>();
  viewport_state.viewportCount = 1;
  viewport_state.scissorCount = 1;

  auto dynamic_info = vkmk_zero<VkPipelineDynamicStateCreateInfo>();
  dynamic_info.dynamicStateCount = (u32)dynamic_state.size();
  dynamic_info.pDynamicStates = dynamic_state.data();

  // Unused, default to zero
  static constexpr auto vertex_input = vkmk_zero<VkPipelineVertexInputStateCreateInfo>();

  // Dummy blending, for now
  auto color_blending = vkmk_zero<VkPipelineColorBlendStateCreateInfo>();
  color_blending.logicOpEnable = VK_FALSE;
  color_blending.logicOp = VK_LOGIC_OP_COPY;
  color_blending.attachmentCount = 1;
  color_blending.pAttachments = &_blend_attachment;

  auto pipeline_info = vkmk_zero<VkGraphicsPipelineCreateInfo>(&_rendering_info);
  pipeline_info.stageCount = (u32)_shader_stages.size();
  pipeline_info.pStages = _shader_stages.data();
  pipeline_info.pVertexInputState = &vertex_input;
  pipeline_info.pInputAssemblyState = &_input_assembly;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &_rasterizer;
  pipeline_info.pMultisampleState = &_multisampling;
  pipeline_info.pColorBlendState = &color_blending;
  pipeline_info.pDepthStencilState = &_depth_stencil;
  pipeline_info.layout = _layout;
  pipeline_info.pDynamicState = &dynamic_info;

  VkPipeline pipeline;
  KA_VK_UNEX(
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, vkalloc, &pipeline));

  return {in_place, pipeline};
}

fn VulkanPipelineBuilder::set_layout(VkPipelineLayout layout) -> VulkanPipelineBuilder& {
  _layout = layout;
  return *this;
}

fn VulkanPipelineBuilder::set_shaders(VkShaderModule vertex, VkShaderModule fragment)
  -> VulkanPipelineBuilder& {
  _shader_stages.clear();
  _shader_stages.push_back(vkmk_pipeline_stage_info(VK_SHADER_STAGE_VERTEX_BIT, vertex));
  _shader_stages.push_back(vkmk_pipeline_stage_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragment));
  return *this;
}

fn VulkanPipelineBuilder::set_topology(VkPrimitiveTopology topology) -> VulkanPipelineBuilder& {
  _input_assembly.topology = topology;
  _input_assembly.primitiveRestartEnable = VK_FALSE; // not used
  return *this;
}

fn VulkanPipelineBuilder::set_poly_mode(VkPolygonMode mode, f32 width) -> VulkanPipelineBuilder& {
  _rasterizer.polygonMode = mode;
  _rasterizer.lineWidth = width;
  return *this;
}

fn VulkanPipelineBuilder::set_cull_mode(VkCullModeFlags mode, VkFrontFace front_face)
  -> VulkanPipelineBuilder& {
  _rasterizer.cullMode = mode;
  _rasterizer.frontFace = front_face;
  return *this;
}

fn VulkanPipelineBuilder::set_color_format(VkFormat format) -> VulkanPipelineBuilder& {
  _color_format = format;
  _rendering_info.colorAttachmentCount = 1;
  _rendering_info.pColorAttachmentFormats = &_color_format;
  return *this;
}

fn VulkanPipelineBuilder::set_depth_format(VkFormat format) -> VulkanPipelineBuilder& {
  _rendering_info.depthAttachmentFormat = format;
  return *this;
}

fn VulkanPipelineBuilder::disable_multisampling() -> VulkanPipelineBuilder& {
  // No multisampling = 1 sample per pixel + no alpha coverage
  _multisampling.sampleShadingEnable = VK_FALSE;
  _multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  _multisampling.minSampleShading = 1.f;
  _multisampling.pSampleMask = nullptr;
  _multisampling.alphaToCoverageEnable = VK_FALSE;
  _multisampling.alphaToOneEnable = VK_FALSE;
  return *this;
}

fn VulkanPipelineBuilder::disable_blending() -> VulkanPipelineBuilder& {
  // default write mask + no blending
  _blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  _blend_attachment.blendEnable = VK_FALSE;
  return *this;
}

fn VulkanPipelineBuilder::disable_depth_test() -> VulkanPipelineBuilder& {
  _depth_stencil.depthTestEnable = VK_FALSE;
  _depth_stencil.depthWriteEnable = VK_FALSE;
  _depth_stencil.depthCompareOp = VK_COMPARE_OP_NEVER;
  _depth_stencil.depthBoundsTestEnable = VK_FALSE;
  _depth_stencil.stencilTestEnable = VK_FALSE;
  std::memset(&_depth_stencil.front, 0x00, sizeof(_depth_stencil.front));
  std::memset(&_depth_stencil.back, 0x00, sizeof(_depth_stencil.back));
  _depth_stencil.minDepthBounds = 0.f;
  _depth_stencil.maxDepthBounds = 1.f;
  return *this;
}

} // namespace kappa::render
