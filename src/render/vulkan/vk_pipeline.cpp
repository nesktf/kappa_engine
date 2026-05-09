#include "./vk_pipeline.hpp"

#include "./vk_context.hpp"
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

fn vk_create_compute_pipeline(VkDevice device, VkPipelineLayout layout, Span<const u8> src)
  -> VkExpect<VkPipeline> {
  return vk_create_shader(device, {src.data(), src.size()})
    .and_then([&](VkShaderModule shader) -> VkExpect<VkPipeline> {
      const DeferFn defer = [&]() {
        vkDestroyShaderModule(device, shader, vkalloc);
      };
      auto stage_info = vkmk_zero<VkPipelineShaderStageCreateInfo>();
      stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
      stage_info.module = shader;
      stage_info.pName = "main";

      auto compute_info = vkmk_zero<VkComputePipelineCreateInfo>();
      compute_info.layout = layout;
      compute_info.stage = stage_info;

      VkPipeline pip{};
      KA_VK_UNEX(
        vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &compute_info, vkalloc, &pip));
      return {in_place, pip};
    });
}

} // namespace kappa::render

VkResult ka_vk_create_pipeline_builder(ka_VulkanPipelineBuilder* builder) {
  (*builder) = new ka_VulkanPipelineBuilder_impl();
  ka_vk_pipb_clear(*builder);
  return VK_SUCCESS;
}

void ka_vk_destroy_pipeline_builder(ka_VulkanPipelineBuilder builder) {
  if (!builder) {
    return;
  }
  delete builder;
}

namespace {

using namespace kappa;
using kappa::render::VkExpect;
using kappa::render::vkmk_zero;

fn build_pipeline(ka_VulkanPipelineBuilder_impl& pipb, ka_VulkanPipeline& pipeline,
                  VkDevice device) -> VkExpect<void> {
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
  color_blending.pAttachments = &pipb.blend_attachment;

  auto pipeline_info = vkmk_zero<VkGraphicsPipelineCreateInfo>(&pipb.rendering_info);
  pipeline_info.stageCount = (u32)pipb.shader_stages.size();
  pipeline_info.pStages = pipb.shader_stages.data();
  pipeline_info.pVertexInputState = &vertex_input;
  pipeline_info.pInputAssemblyState = &pipb.input_assembly;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &pipb.rasterizer;
  pipeline_info.pMultisampleState = &pipb.multisampling;
  pipeline_info.pColorBlendState = &color_blending;
  pipeline_info.pDepthStencilState = &pipb.depth_stencil;
  pipeline_info.layout = pipb.layout;
  pipeline_info.pDynamicState = &dynamic_info;

  KA_VK_UNEX(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, vkalloc,
                                       &pipeline.pipeline));
  pipeline.layout = pipb.layout;

  return {};
}

} // namespace

VkResult ka_vk_pipb_build(ka_VulkanPipelineBuilder pipb, ka_VulkanContext vk,
                          ka_VulkanPipeline* pip) {
  ka_assert(pipb);
  ka_assert(vk);
  ka_assert(pip);
  const auto device = vk->device->device();
  std::memset(pip, 0x00, sizeof(*pip));
  pip->vk = vk;
  return build_pipeline(*pipb, *pip, device).error_or(kappa::render::VkError(VK_SUCCESS)).code();
}

void ka_vk_pipb_clear(ka_VulkanPipelineBuilder pipb) {
  ka_assert(pipb);
  pipb->input_assembly = vkmk_zero<decltype(pipb->input_assembly)>();
  pipb->rasterizer = vkmk_zero<decltype(pipb->rasterizer)>();
  std::memset(&pipb->blend_attachment, 0x00, sizeof(pipb->blend_attachment));
  pipb->multisampling = vkmk_zero<decltype(pipb->multisampling)>();
  pipb->layout = VK_NULL_HANDLE;
  pipb->depth_stencil = vkmk_zero<decltype(pipb->depth_stencil)>();
  pipb->rendering_info = vkmk_zero<decltype(pipb->rendering_info)>();
  pipb->shader_stages.clear();
}

void ka_vk_pipb_set_layout(ka_VulkanPipelineBuilder pipb, VkPipelineLayout layout) {
  ka_assert(pipb);
  pipb->layout = layout;
}

void ka_vk_pipb_add_shader(ka_VulkanPipelineBuilder pipb,
                           ka_VulkanShaderModule const* shader_module) {
  pipb->shader_stages.push_back(
    kappa::render::vkmk_pipeline_stage_info(shader_module->stage, shader_module->shader));
}

void ka_vk_pipb_set_topology(ka_VulkanPipelineBuilder pipb, VkPrimitiveTopology topology) {
  pipb->input_assembly.topology = topology;
  pipb->input_assembly.primitiveRestartEnable = VK_FALSE; // not used
}

void ka_vk_pipb_set_poly_mode(ka_VulkanPipelineBuilder pipb, VkPolygonMode mode, float width) {
  pipb->rasterizer.polygonMode = mode;
  pipb->rasterizer.lineWidth = width;
}

void ka_vk_pipb_set_cull_mode(ka_VulkanPipelineBuilder pipb, VkCullModeFlags mode,
                              VkFrontFace front_face) {
  pipb->rasterizer.cullMode = mode;
  pipb->rasterizer.frontFace = front_face;
}

void ka_vk_pipb_set_color_format(ka_VulkanPipelineBuilder pipb, VkFormat format) {
  pipb->color_format = format;
  pipb->rendering_info.colorAttachmentCount = 1;
  pipb->rendering_info.pColorAttachmentFormats = &pipb->color_format;
}

void ka_vk_pipb_set_depth_format(ka_VulkanPipelineBuilder pipb, VkFormat format) {
  pipb->rendering_info.depthAttachmentFormat = format;
}

void ka_vk_pipb_disable_ms(ka_VulkanPipelineBuilder pipb) {
  // No multisampling = 1 sample per pixel + no alpha coverage
  pipb->multisampling.sampleShadingEnable = VK_FALSE;
  pipb->multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  pipb->multisampling.minSampleShading = 1.f;
  pipb->multisampling.pSampleMask = nullptr;
  pipb->multisampling.alphaToCoverageEnable = VK_FALSE;
  pipb->multisampling.alphaToOneEnable = VK_FALSE;
}

void ka_vk_pipb_disable_blending(ka_VulkanPipelineBuilder pipb) {
  // default write mask + no blending
  pipb->blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  pipb->blend_attachment.blendEnable = VK_FALSE;
}

void ka_vk_pipb_disable_depth_test(ka_VulkanPipelineBuilder pipb) {

  pipb->depth_stencil.depthTestEnable = VK_FALSE;
  pipb->depth_stencil.depthWriteEnable = VK_FALSE;
  pipb->depth_stencil.depthCompareOp = VK_COMPARE_OP_NEVER;
  pipb->depth_stencil.depthBoundsTestEnable = VK_FALSE;
  pipb->depth_stencil.stencilTestEnable = VK_FALSE;
  std::memset(&pipb->depth_stencil.front, 0x00, sizeof(pipb->depth_stencil.front));
  std::memset(&pipb->depth_stencil.back, 0x00, sizeof(pipb->depth_stencil.back));
  pipb->depth_stencil.minDepthBounds = 0.f;
  pipb->depth_stencil.maxDepthBounds = 1.f;
}
