#include "./vk_pipeline.hpp"

#include "./vk_private.hpp"
#include "./vk_util.hpp"
#include <vulkan/vulkan_core.h>

namespace kappa::render {

fn VkDescAlloc::create(VkDevice device, u32 max_sets, Span<const VkDescPoolRatio> ratios)
  -> VkExpect<VkDescAlloc> {
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

fn VkDescAlloc::destroy() -> void {
  vkDestroyDescriptorPool(self.device, self.pool, vkalloc);
}

fn VkDescAlloc::allocate(VkDescriptorSetLayout layout) -> VkExpect<VkDescriptorSet> {
  ka_assert(layout != VK_NULL_HANDLE);
  auto alloc_info = vkmk_zero<VkDescriptorSetAllocateInfo>();
  alloc_info.descriptorPool = self.pool;
  alloc_info.descriptorSetCount = 1;
  alloc_info.pSetLayouts = &layout;

  VkDescriptorSet set;
  KA_VK_UNEX(vkAllocateDescriptorSets(self.device, &alloc_info, &set));
  return {in_place, set};
}

fn VkDescAlloc::clear() -> void {
  vkResetDescriptorPool(self.device, self.pool, 0);
}

namespace {

constexpr u32 MAX_SETS_PER_POOL = 4092;
constexpr f32 POOL_MULTIPLIER = 1.5f;

fn dyn_desc_create_pool(VkDevice device, u32 set_count, Span<const VkDescPoolRatio> ratios)
  -> VkExpect<VkDescriptorPool> {
  ka_assert(device != VK_NULL_HANDLE);

  Vec<VkDescriptorPoolSize> pool_sizes;
  pool_sizes.reserve(ratios.size());
  for (const auto& [type, ratio] : ratios) {
    VkDescriptorPoolSize size{};
    size.type = type;
    size.descriptorCount = (u32)(ratio * set_count);
    pool_sizes.push_back(size);
  }

  auto pool_info = vkmk_zero<VkDescriptorPoolCreateInfo>();
  pool_info.maxSets = set_count;
  pool_info.poolSizeCount = (u32)pool_sizes.size();
  pool_info.pPoolSizes = pool_sizes.data();
  VkDescriptorPool new_pool;
  KA_VK_UNEX(vkCreateDescriptorPool(device, &pool_info, vkalloc, &new_pool));
  return {in_place, new_pool};
}

fn dyn_desc_get_pool(VkDynDescAlloc::Self& self) -> VkExpect<VkDescriptorPool> {
  VkDescriptorPool pool = VK_NULL_HANDLE;
  if (!self.avail.empty()) {
    pool = self.avail.back();
    self.avail.pop_back();
  } else {
    auto new_pool = dyn_desc_create_pool(self.device, self.sets_per_pool,
                                         {self.ratios.data(), self.ratios.size()});
    if (!new_pool) {
      return {unexpect, new_pool.error()};
    }
    pool = *new_pool;
    self.sets_per_pool = std::min((u32)(self.sets_per_pool * POOL_MULTIPLIER), MAX_SETS_PER_POOL);
  }
  return {in_place, pool};
}

} // namespace

fn VkDynDescAlloc::create(VkDevice device, u32 initial_sets, Span<const VkDescPoolRatio> ratios)
  -> VkExpect<VkDynDescAlloc> {
  ka_assert(device != VK_NULL_HANDLE);

  Vec<VkDescPoolRatio> pool_ratios(ratios.begin(), ratios.end());
  Vec<VkDescriptorPool> full;
  Vec<VkDescriptorPool> avail;
  avail.reserve(1);
  u32 sets_per_pool = (u32)(initial_sets * POOL_MULTIPLIER);

  auto new_pool = dyn_desc_create_pool(device, initial_sets, ratios);
  if (!new_pool) {
    return {unexpect, new_pool.error()};
  }
  avail.push_back(*new_pool);
  return {
    in_place,        create_t(), std::move(pool_ratios), std::move(avail),
    std::move(full), device,     sets_per_pool,
  };
}

fn VkDynDescAlloc::allocate(VkDescriptorSetLayout layout, void* pNext)
  -> VkExpect<VkDescriptorSet> {
  ka_assert(layout != VK_NULL_HANDLE);

  auto pool = dyn_desc_get_pool(self);
  if (!pool) {
    return {unexpect, pool.error()};
  }

  auto alloc_info = vkmk_zero<VkDescriptorSetAllocateInfo>(pNext);
  alloc_info.descriptorPool = *pool;
  alloc_info.descriptorSetCount = 1;
  alloc_info.pSetLayouts = &layout;

  VkDescriptorSet set;
  auto res = vkAllocateDescriptorSets(self.device, &alloc_info, &set);
  if (res == VK_ERROR_OUT_OF_POOL_MEMORY || res == VK_ERROR_FRAGMENTED_POOL) {
    self.full.push_back(*pool);
    pool = dyn_desc_get_pool(self);
    if (!pool) {
      return {unexpect, pool.error()};
    }

    KA_VK_UNEX(vkAllocateDescriptorSets(self.device, &alloc_info, &set));
  }
  self.avail.push_back(*pool);

  return {in_place, set};
}

fn VkDynDescAlloc::clear() -> void {
  for (auto& pool : self.avail) {
    KA_VK_LOG(verbose, "Deleting DESCPOOL {}", fmt::ptr(pool));
    vkResetDescriptorPool(self.device, pool, 0);
  }
  for (auto& pool : self.full) {
    KA_VK_LOG(verbose, "Deleting DESCPOOL {}", fmt::ptr(pool));
    vkResetDescriptorPool(self.device, pool, 0);
    self.avail.push_back(pool);
  }
  self.full.clear();
}

fn VkDynDescAlloc::destroy() -> void {
  for (auto& pool : self.avail) {
    vkDestroyDescriptorPool(self.device, pool, vkalloc);
  }
  for (auto& pool : self.full) {
    vkDestroyDescriptorPool(self.device, pool, vkalloc);
  }
}

VkGfxPipelineBuilder::VkGfxPipelineBuilder() {
  clear();
}

VkDescWriter::VkDescWriter(VkDevice device) : _device(device) {}

fn VkDescWriter::write_image(s32 binding, VkImageView view, VkImageLayout layout,
                             VkSampler sampler, VkDescImageType type) -> void {
  auto& info = _image_infos.emplace_back(sampler, view, layout);
  auto write = vkmk_zero<VkWriteDescriptorSet>();
  write.dstBinding = binding;
  write.dstSet = VK_NULL_HANDLE; // set on update_set()
  write.descriptorCount = 1;
  write.descriptorType = (VkDescriptorType)type;
  write.pImageInfo = &info; // should be fine if we are using a deque (right?)
  _writes.push_back(write);
}

fn VkDescWriter::write_buffer(s32 binding, VkBuffer buffer, usize size, usize offset,
                              VkDescBuffType type) -> void {
  auto& info = _buff_infos.emplace_back(buffer, offset, size);
  auto write = vkmk_zero<VkWriteDescriptorSet>();
  write.dstBinding = binding;
  write.dstSet = VK_NULL_HANDLE; // set on update_set()
  write.descriptorCount = 1;
  write.descriptorType = (VkDescriptorType)type;
  write.pBufferInfo = &info; // should be fine if we are using a deque (right?)
  _writes.push_back(write);
}

fn VkDescWriter::clear() -> void {
  _image_infos.clear();
  _buff_infos.clear();
  _writes.clear();
}

fn VkDescWriter::update_set(VkDescriptorSet set) -> void {
  for (auto& write : _writes) {
    write.dstSet = set;
  }
  vkUpdateDescriptorSets(_device, (u32)_writes.size(), _writes.data(), 0, nullptr);
}

namespace {

fn stage_idx(VkShaderStageFlagBits stage) -> u32 {
  switch (stage) {
    case VK_SHADER_STAGE_VERTEX_BIT:
      return 0;
    case VK_SHADER_STAGE_FRAGMENT_BIT:
      return 1;
    case VK_SHADER_STAGE_GEOMETRY_BIT:
      return 2;
    case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
      return 3;
    case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
      return 4;
    default:
      return 0;
  }
}

fn collect_stages(
  const std::array<VkGfxPipelineBuilder::ShaderEntry, VkGfxPipelineBuilder::STAGE_COUNT>& shaders)
  -> std::pair<std::array<VkPipelineShaderStageCreateInfo, VkGfxPipelineBuilder::STAGE_COUNT>,
               u32> {
  static constexpr auto shader_usage = std::to_array({
    VK_SHADER_STAGE_VERTEX_BIT,
    VK_SHADER_STAGE_FRAGMENT_BIT,
    VK_SHADER_STAGE_GEOMETRY_BIT,
    VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
    VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
  });
  std::array<VkPipelineShaderStageCreateInfo, 5> shader_stages;
  u32 shader_count = 0;
  for (usize i = 0; i < shaders.size(); ++i) {
    if (shaders[i].shader != VK_NULL_HANDLE) {
      shader_stages[shader_count++] =
        vkmk_pipeline_stage_info(shader_usage[i], shaders[i].shader, shaders[i].entrypoint);
    }
  }
  return {shader_stages, shader_count};
}

} // namespace

fn VkGfxPipelineBuilder::build(VkContext_Impl& vk) -> VkExpect<VkPipeline> {
  // Dynamic viewport & scissor
  static constexpr auto dynamic_state =
    std::to_array<VkDynamicState>({VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});
  auto viewport_state = vkmk_zero<VkPipelineViewportStateCreateInfo>();
  viewport_state.viewportCount = 1;
  viewport_state.scissorCount = 1;

  auto dynamic_info = vkmk_zero<VkPipelineDynamicStateCreateInfo>();
  dynamic_info.dynamicStateCount = (u32)dynamic_state.size();
  dynamic_info.pDynamicStates = dynamic_state.data();

  const auto [shader_stages, shader_count] = collect_stages(_shader_stages);

  // Unused, default to zero
  static constexpr auto vertex_input = vkmk_zero<VkPipelineVertexInputStateCreateInfo>();

  // Dummy blending, for now
  auto color_blending = vkmk_zero<VkPipelineColorBlendStateCreateInfo>();
  color_blending.logicOpEnable = VK_FALSE;
  color_blending.logicOp = VK_LOGIC_OP_COPY;
  color_blending.attachmentCount = 1;
  color_blending.pAttachments = &_blend_attachment;

  auto pipeline_info = vkmk_zero<VkGraphicsPipelineCreateInfo>(&_rendering_info);
  pipeline_info.stageCount = shader_count;
  pipeline_info.pStages = shader_stages.data();
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
  KA_VK_UNEX(vkCreateGraphicsPipelines(vk.device.device(), VK_NULL_HANDLE, 1, &pipeline_info,
                                       vkalloc, &pipeline));
  return {in_place, pipeline};
}

fn VkGfxPipelineBuilder::clear() -> void {
  _input_assembly = vkmk_zero<decltype(_input_assembly)>();
  _rasterizer = vkmk_zero<decltype(_rasterizer)>();
  _multisampling = vkmk_zero<decltype(_multisampling)>();
  _depth_stencil = vkmk_zero<decltype(_depth_stencil)>();
  _rendering_info = vkmk_zero<decltype(_rendering_info)>();
  std::memset(&_blend_attachment, 0x00, sizeof(_blend_attachment));
  std::memset(_shader_stages.data(), 0x00, sizeof(_shader_stages));
  _layout = VK_NULL_HANDLE;
}

fn VkGfxPipelineBuilder::add_module(VkShaderStageFlagBits stage, VkShaderModule shader,
                                    const char* entrypoint) -> VkGfxPipelineBuilder& {
  auto& pos = _shader_stages[stage_idx(stage)];
  pos.shader = shader;
  pos.entrypoint = entrypoint ? entrypoint : "main";
  return *this;
}

fn VkGfxPipelineBuilder::set_layout(VkPipelineLayout layout) -> VkGfxPipelineBuilder& {
  _layout = layout;
  return *this;
}

fn VkGfxPipelineBuilder::set_topology(VkPrimitiveTopology topology) -> VkGfxPipelineBuilder& {
  _input_assembly.topology = topology;
  _input_assembly.primitiveRestartEnable = VK_FALSE; // not used
  return *this;
}

fn VkGfxPipelineBuilder::set_poly_mode(VkPolygonMode mode, f32 width) -> VkGfxPipelineBuilder& {
  _rasterizer.polygonMode = mode;
  _rasterizer.lineWidth = width;
  return *this;
}

fn VkGfxPipelineBuilder::set_cull_mode(VkCullModeFlags mode, VkFrontFace front_face)
  -> VkGfxPipelineBuilder& {
  _rasterizer.cullMode = mode;
  _rasterizer.frontFace = front_face;
  return *this;
}

fn VkGfxPipelineBuilder::set_color_format(VkFormat format) -> VkGfxPipelineBuilder& {
  _color_format = format;
  _rendering_info.colorAttachmentCount = 1;
  _rendering_info.pColorAttachmentFormats = &_color_format;
  return *this;
}

fn VkGfxPipelineBuilder::set_depth_format(VkFormat format) -> VkGfxPipelineBuilder& {
  _rendering_info.depthAttachmentFormat = format;
  return *this;
}

fn VkGfxPipelineBuilder::disable_multisampling() -> VkGfxPipelineBuilder& {
  // 1 sample per pixel + no alpha coverage
  _multisampling.sampleShadingEnable = VK_FALSE;
  _multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  _multisampling.minSampleShading = 1.f;
  _multisampling.pSampleMask = nullptr;
  _multisampling.alphaToCoverageEnable = VK_FALSE;
  _multisampling.alphaToOneEnable = VK_FALSE;
  return *this;
}

fn VkGfxPipelineBuilder::disable_blending() -> VkGfxPipelineBuilder& {
  // default write mask + no blending
  _blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  _blend_attachment.blendEnable = VK_FALSE;
  return *this;
}

fn VkGfxPipelineBuilder::enable_blending_additive() -> VkGfxPipelineBuilder& {
  _blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  _blend_attachment.blendEnable = VK_TRUE;
  _blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  _blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
  _blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
  _blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  _blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  _blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
  return *this;
}

fn VkGfxPipelineBuilder::enable_blending_alphablend() -> VkGfxPipelineBuilder& {

  _blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  _blend_attachment.blendEnable = VK_TRUE;
  _blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  _blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  _blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
  _blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  _blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  _blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
  return *this;
}

fn VkGfxPipelineBuilder::disable_depth_test() -> VkGfxPipelineBuilder& {
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

fn VkGfxPipelineBuilder::enable_depth_test(VkDepthWriteEnable enable_write, VkCompareOp op)
  -> VkGfxPipelineBuilder& {
  _depth_stencil.depthTestEnable = VK_TRUE;
  _depth_stencil.depthWriteEnable = (VkBool32)enable_write;
  _depth_stencil.depthCompareOp = op;
  _depth_stencil.depthBoundsTestEnable = VK_FALSE;
  _depth_stencil.stencilTestEnable = VK_FALSE;
  _depth_stencil.front = {};
  _depth_stencil.back = {};
  _depth_stencil.minDepthBounds = 0.f;
  _depth_stencil.maxDepthBounds = 1.f;
  return *this;
}

fn vk_create_shader(VkContext_Impl& vk, Span<const u8> src) -> VkExpect<VkShaderModule> {
  auto info = vkmk_zero<VkShaderModuleCreateInfo>();
  info.codeSize = src.size();
  info.pCode = reinterpret_cast<const u32*>(src.data());
  VkShaderModule shader;
  if (auto ret = vkCreateShaderModule(vk.device.device(), &info, vkalloc, &shader);
      ret != VK_SUCCESS) {
    return {unexpect, ret};
  }
  return {in_place, shader};
}

fn vk_destroy_shader(VkContext_Impl& vk, VkShaderModule shader) noexcept -> void {
  if (shader == VK_NULL_HANDLE) {
    return;
  }
  vkDestroyShaderModule(vk.device.device(), shader, vkalloc);
}

VkDescLayoutBuilder::VkDescLayoutBuilder() {
  clear();
}

fn VkDescLayoutBuilder::build(VkContext_Impl& vk, VkShaderStageFlags stages, void* pNext,
                              VkDescriptorSetLayoutCreateFlags flags)
  -> VkExpect<VkDescriptorSetLayout> {
  for (auto& binding : _bindings) {
    binding.stageFlags |= stages;
  }
  auto info = vkmk_zero<VkDescriptorSetLayoutCreateInfo>(pNext);
  info.pBindings = _bindings.data();
  info.bindingCount = (u32)_bindings.size();
  info.flags = flags;

  VkDescriptorSetLayout set;
  KA_VK_UNEX(vkCreateDescriptorSetLayout(vk.device.device(), &info, nullptr, &set));
  return {in_place, set};
}

fn VkDescLayoutBuilder::clear() -> void {
  _bindings.clear();
}

fn VkDescLayoutBuilder::add_binding(u32 binding, VkDescriptorType type) -> VkDescLayoutBuilder& {
  VkDescriptorSetLayoutBinding bind{};
  bind.binding = binding;
  bind.descriptorCount = 1;
  bind.descriptorType = type;
  _bindings.push_back(bind);
  return *this;
}

fn vk_destroy_desc_layout(VkContext_Impl& vk, VkDescriptorSetLayout layout) noexcept -> void {
  if (layout == VK_NULL_HANDLE) {
    return;
  }
  vkDestroyDescriptorSetLayout(vk.device.device(), layout, vkalloc);
}

VkPipelineLayoutBuilder::VkPipelineLayoutBuilder() {
  clear();
}

fn VkPipelineLayoutBuilder::build(VkContext_Impl& vk, VkPipelineLayoutCreateFlags flags)
  -> VkExpect<VkPipelineLayout> {
  auto layout_info = vkmk_zero<VkPipelineLayoutCreateInfo>();
  layout_info.flags = flags;
  layout_info.pPushConstantRanges = _con_range.data();
  layout_info.pushConstantRangeCount = _con_range.size();
  layout_info.pSetLayouts = _set_layouts.data();
  layout_info.setLayoutCount = _set_layouts.size();
  VkPipelineLayout layout;
  KA_VK_UNEX(vkCreatePipelineLayout(vk.device.device(), &layout_info, vkalloc, &layout));
  return {in_place, layout};
}

fn VkPipelineLayoutBuilder::clear() -> void {
  _set_layouts.clear();
  _con_range.clear();
}

fn VkPipelineLayoutBuilder::add_push_range(VkShaderStageFlags flags, u32 size, u32 offset)
  -> VkPipelineLayoutBuilder& {
  VkPushConstantRange range{};
  range.offset = offset;
  range.size = size;
  range.stageFlags = flags;
  _con_range.push_back(range);
  return *this;
}

fn VkPipelineLayoutBuilder::add_layout(VkDescriptorSetLayout layout) -> VkPipelineLayoutBuilder& {
  _set_layouts.push_back(layout);
  return *this;
}

fn vk_destroy_pipeline_layout(VkContext_Impl& vk, VkPipelineLayout layout) noexcept -> void {
  if (layout == VK_NULL_HANDLE) {
    return;
  }
  vkDestroyPipelineLayout(vk.device.device(), layout, vkalloc);
}

fn vk_destroy_pipeline(VkContext_Impl& vk, VkPipeline pipeline) noexcept -> void {
  if (pipeline == VK_NULL_HANDLE) {
    return;
  }
  vkDestroyPipeline(vk.device.device(), pipeline, vkalloc);
}

fn vk_create_compute_pipeline(VkContext_Impl& vk, VkPipelineLayout layout, VkShaderModule shader,
                              const char* entrypoint) -> VkExpect<VkPipeline> {
  const auto stage_info = vkmk_pipeline_stage_info(VK_SHADER_STAGE_COMPUTE_BIT, shader,
                                                   entrypoint ? entrypoint : "main");
  auto compute_info = vkmk_zero<VkComputePipelineCreateInfo>();
  compute_info.layout = layout;
  compute_info.stage = stage_info;

  VkPipeline pip;
  KA_VK_UNEX(
    vkCreateComputePipelines(vk.device.device(), VK_NULL_HANDLE, 1, &compute_info, vkalloc, &pip));
  return {in_place, pip};
}

} // namespace kappa::render
