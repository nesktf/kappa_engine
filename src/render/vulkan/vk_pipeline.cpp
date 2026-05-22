#include "./vk_pipeline.hpp"

#include "./vk_context.hpp"
#include "./vk_util.hpp"
#include "render/vulkan/vk.h"
#include "render/vulkan/vk_error.hpp"
#include <vulkan/vulkan_core.h>

namespace kappa::render {

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

fn VulkanDescPool::alloc_sets(VkDescriptorSetLayout layout, VkDescriptorSet* sets, u32 count)
  -> VkExpect<void> {
  auto alloc_info = vkmk_zero<VkDescriptorSetAllocateInfo>();
  alloc_info.descriptorPool = _pool;
  alloc_info.descriptorSetCount = count;
  alloc_info.pSetLayouts = &layout;

  KA_VK_UNEX(vkAllocateDescriptorSets(_device, &alloc_info, sets));

  return {};
}

fn VulkanDescPool::clear() -> void {
  vkResetDescriptorPool(_device, _pool, 0);
}

} // namespace kappa::render

using namespace kappa;
using namespace kappa::render;

namespace {

fn create_shader(VkDevice device, Span<const u8> src) -> VkExpect<VkShaderModule> {
  auto info = vkmk_zero<VkShaderModuleCreateInfo>();
  info.codeSize = src.size();
  info.pCode = reinterpret_cast<const u32*>(src.data());
  VkShaderModule shader;
  if (auto ret = vkCreateShaderModule(device, &info, vkalloc, &shader); ret != VK_SUCCESS) {
    return {unexpect, ret};
  }
  return {in_place, shader};
}

} // namespace

VkResult ka_vk_create_shader(ka_VkContext vk, VkShaderModule* shader,
                             ka_VkShaderArgs const* args) {
  ka_assert(vk);
  ka_assert(shader);
  ka_assert(args);
  auto shad = create_shader(vk->device.device(), {args->src, args->src_size});
  if (shad) {
    if (args->lifetime == KA_VK_LIFETIME_AUTO) {
      vk->delqueue.enqueue(*shad, vk->device.device());
    } else if (args->lifetime == KA_VK_LIFETIME_AUTO_FRAME) {
      vk->framedata.curr_frame().delqueue.enqueue(*shad, vk->device.device());
    }
    (*shader) = *shad;
  }
  return shad.error_or(render::VkError(VK_SUCCESS)).code();
}

void ka_vk_destroy_shader(ka_VkContext vk, VkShaderModule shader) {
  if (!vk || shader == VK_NULL_HANDLE) {
    return;
  }
  vkDestroyShaderModule(vk->device.device(), shader, vkalloc);
}

VkResult ka_vk_create_dscset_layout(ka_VkContext vk, VkDescriptorSetLayout* layout,
                                    ka_VkDscSetLayoutArgs const* args) {
  ka_assert(vk);
  ka_assert(layout);
  ka_assert(args);

  auto info = vkmk_zero<VkDescriptorSetLayoutCreateInfo>();
  info.pBindings = args->bindings;
  info.bindingCount = args->binding_count;
  info.flags = args->flags;

  auto ret = vkCreateDescriptorSetLayout(vk->device.device(), &info, nullptr, layout);
  if (ret) {
    return ret;
  }

  if (args->lifetime == KA_VK_LIFETIME_AUTO) {
    vk->delqueue.enqueue(*layout, vk->device.device());
  } else if (args->lifetime == KA_VK_LIFETIME_AUTO_FRAME) {
    vk->framedata.curr_frame().delqueue.enqueue(*layout, vk->device.device());
  }
  return ret;
}

void ka_vk_destroy_dscset_layout(ka_VkContext vk, VkDescriptorSetLayout layout) {
  if (!vk || layout == VK_NULL_HANDLE) {
    return;
  }
  vkDestroyDescriptorSetLayout(vk->device.device(), layout, vkalloc);
}

VkResult ka_vk_alloc_dscsets(ka_VkContext vk, VkDescriptorSet* dscsets, uint32_t count,
                             VkDescriptorSetLayout layout, ka_VkLifetime lifetime) {

  ka_assert(vk);
  ka_assert(dscsets && count > 0);
  ka_assert(layout != VK_NULL_HANDLE);
  KA_UNUSED(lifetime); // TODO
  VkResult ret =
    vk->descpool.alloc_sets(layout, dscsets, count).error_or(render::VkError(VK_SUCCESS)).code();
  return ret;
}

void ka_vk_dealloc_dscsets(ka_VkContext vk, VkDescriptorSet* dscsets, uint32_t count) {
  if (!vk || !dscsets || !count) {
    return;
  }
  // TODO
}

void ka_vk_update_dscset(ka_VkContext vk, VkWriteDescriptorSet const* write_sets,
                         uint32_t write_sets_count) {
  vkUpdateDescriptorSets(vk->device.device(), write_sets_count, write_sets, 0, 0);
}

VkResult ka_vk_create_pipeline_layout(ka_VkContext vk, VkPipelineLayout* layout,
                                      ka_VkPipelineLayoutArgs const* args) {
  ka_assert(vk);
  ka_assert(layout);
  ka_assert(args);

  auto layout_info = vkmk_zero<VkPipelineLayoutCreateInfo>();
  layout_info.flags = args->flags;
  layout_info.pPushConstantRanges = args->push_constant_ranges;
  layout_info.pushConstantRangeCount = args->push_constant_range_count;
  layout_info.pSetLayouts = args->set_layouts;
  layout_info.setLayoutCount = args->set_layout_count;
  auto ret = vkCreatePipelineLayout(vk->device.device(), &layout_info, vkalloc, layout);
  if (ret) {
    return ret;
  }

  if (args->lifetime == KA_VK_LIFETIME_AUTO) {
    vk->delqueue.enqueue(*layout, vk->device.device());
  } else if (args->lifetime == KA_VK_LIFETIME_AUTO_FRAME) {
    vk->framedata.curr_frame().delqueue.enqueue(*layout, vk->device.device());
  }

  return ret;
}

void ka_vk_destroy_pipeline_layout(ka_VkContext vk, VkPipelineLayout layout) {
  if (!vk || layout == VK_NULL_HANDLE) {
    return;
  }
  vkDestroyPipelineLayout(vk->device.device(), layout, vkalloc);
}

namespace {

struct VkPipelineBuildData {
  VkPipelineShaderStageCreateInfo const* shader_stages;
  u32 shader_stage_count;
  VkPipelineInputAssemblyStateCreateInfo input_assembly;
  VkPipelineRasterizationStateCreateInfo rasterizer;
  VkPipelineColorBlendAttachmentState blend_attachment;
  VkPipelineMultisampleStateCreateInfo multisampling;
  VkPipelineLayout layout;
  VkPipelineDepthStencilStateCreateInfo depth_stencil;
  VkPipelineRenderingCreateInfo rendering_info;
  VkFormat color_format;
};

fn build_pipeline(VkPipelineBuildData& pipb, VkPipeline* pipeline, VkDevice device)
  -> VkExpect<void> {
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
  pipeline_info.stageCount = pipb.shader_stage_count;
  pipeline_info.pStages = pipb.shader_stages;
  pipeline_info.pVertexInputState = &vertex_input;
  pipeline_info.pInputAssemblyState = &pipb.input_assembly;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &pipb.rasterizer;
  pipeline_info.pMultisampleState = &pipb.multisampling;
  pipeline_info.pColorBlendState = &color_blending;
  pipeline_info.pDepthStencilState = &pipb.depth_stencil;
  pipeline_info.layout = pipb.layout;
  pipeline_info.pDynamicState = &dynamic_info;

  KA_VK_UNEX(
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, vkalloc, pipeline));

  return {};
}

fn init_pip_build_data(VkPipelineBuildData& pipb) {
  pipb.input_assembly = vkmk_zero<decltype(pipb.input_assembly)>();
  pipb.rasterizer = vkmk_zero<decltype(pipb.rasterizer)>();
  std::memset(&pipb.blend_attachment, 0x00, sizeof(pipb.blend_attachment));
  pipb.multisampling = vkmk_zero<decltype(pipb.multisampling)>();
  pipb.layout = VK_NULL_HANDLE;
  pipb.depth_stencil = vkmk_zero<decltype(pipb.depth_stencil)>();
  pipb.rendering_info = vkmk_zero<decltype(pipb.rendering_info)>();
  pipb.shader_stages = nullptr;
  pipb.shader_stage_count = 0;
}

fn collect_stages(const VkShaderModule (&modules)[KA_VK_MODULE_COUNT])
  -> std::pair<std::array<VkPipelineShaderStageCreateInfo, KA_VK_MODULE_COUNT>, u32> {
  static constexpr auto shader_usage = std::to_array({
    VK_SHADER_STAGE_VERTEX_BIT,
    VK_SHADER_STAGE_FRAGMENT_BIT,
    VK_SHADER_STAGE_GEOMETRY_BIT,
    VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
    VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
  });
  std::array<VkPipelineShaderStageCreateInfo, KA_VK_MODULE_COUNT> shader_stages;
  u32 shader_count = 0;
  for (usize i = 0; i < KA_VK_MODULE_COUNT; ++i) {
    if (modules[i]) {
      shader_stages[shader_count++] = vkmk_pipeline_stage_info(shader_usage[i], modules[i]);
    }
  }
  return {shader_stages, shader_count};
}

} // namespace

VkResult ka_vk_create_pipeline_gfx(ka_VkContext vk, VkPipeline* pipeline,
                                   ka_VkPipelineGfxArgs const* args) {
  ka_assert(vk);
  ka_assert(pipeline);
  ka_assert(args);
  const auto device = vk->device.device();
  const auto [shader_stages, shader_count] = collect_stages(args->shader_modules);

  VkPipelineBuildData build_data;
  init_pip_build_data(build_data);
  build_data.shader_stages = shader_stages.data();
  build_data.shader_stage_count = shader_count;
  build_data.layout = args->layout;

  build_data.input_assembly.topology = args->topology;
  build_data.input_assembly.primitiveRestartEnable = VK_FALSE; // not used

  build_data.color_format = args->color_format;
  build_data.rendering_info.colorAttachmentCount = 1;
  build_data.rendering_info.pColorAttachmentFormats = &build_data.color_format;

  build_data.rasterizer.polygonMode = args->poly_mode;
  build_data.rasterizer.lineWidth = args->poly_width;

  build_data.rasterizer.cullMode = args->cull_mode;
  build_data.rasterizer.frontFace = args->cull_front_face;

  build_data.rendering_info.depthAttachmentFormat = args->depth_format;

  // No multisampling
  // 1 sample per pixel + no alpha coverage
  build_data.multisampling.sampleShadingEnable = VK_FALSE;
  build_data.multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  build_data.multisampling.minSampleShading = 1.f;
  build_data.multisampling.pSampleMask = nullptr;
  build_data.multisampling.alphaToCoverageEnable = VK_FALSE;
  build_data.multisampling.alphaToOneEnable = VK_FALSE;

  // No blending
  // default write mask + no blending
  build_data.blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                               VK_COLOR_COMPONENT_G_BIT |
                                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  build_data.blend_attachment.blendEnable = VK_FALSE;

  // No depth test
  build_data.depth_stencil.depthTestEnable = VK_FALSE;
  build_data.depth_stencil.depthWriteEnable = VK_FALSE;
  build_data.depth_stencil.depthCompareOp = VK_COMPARE_OP_NEVER;
  build_data.depth_stencil.depthBoundsTestEnable = VK_FALSE;
  build_data.depth_stencil.stencilTestEnable = VK_FALSE;
  std::memset(&build_data.depth_stencil.front, 0x00, sizeof(build_data.depth_stencil.front));
  std::memset(&build_data.depth_stencil.back, 0x00, sizeof(build_data.depth_stencil.back));
  build_data.depth_stencil.minDepthBounds = 0.f;
  build_data.depth_stencil.maxDepthBounds = 1.f;

  return build_pipeline(build_data, pipeline, device).error_or(VkError(VK_SUCCESS)).code();
}

VkResult ka_vk_create_pipeline_cmp(ka_VkContext vk, VkPipeline* pipeline,
                                   ka_VkPipelineCmpArgs const* args) {
  const auto stage_info = vkmk_pipeline_stage_info(VK_SHADER_STAGE_COMPUTE_BIT, args->shader);

  auto compute_info = vkmk_zero<VkComputePipelineCreateInfo>();
  compute_info.layout = args->layout;
  compute_info.stage = stage_info;

  auto ret = vkCreateComputePipelines(vk->device.device(), VK_NULL_HANDLE, 1, &compute_info,
                                      vkalloc, pipeline);
  if (ret) {
    return ret;
  }

  if (args->lifetime == KA_VK_LIFETIME_AUTO) {
    vk->delqueue.enqueue(*pipeline, vk->device.device());
  } else if (args->lifetime == KA_VK_LIFETIME_AUTO_FRAME) {
    vk->framedata.curr_frame().delqueue.enqueue(*pipeline, vk->device.device());
  }
  return ret;
}

void ka_vk_destroy_pipeline(ka_VkContext vk, VkPipeline pipeline) {
  if (!vk || pipeline == VK_NULL_HANDLE) {
    return;
  }
  vkDestroyPipeline(vk->device.device(), pipeline, vkalloc);
}
