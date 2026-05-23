#include "./vk_imgui.hpp"

#include "./vk_private.hpp"

#include <imgui_impl_vulkan.h>

namespace kappa::render {

namespace {

VkResult vkmk_imgui_info(VkContext_Impl& vk, VkImGuiInfo& info,
                         const VkDescriptorPoolCreateInfo& descpool_info) {
  VkResult ret =
    vkCreateDescriptorPool(vk.device.device(), &descpool_info, vkalloc, &info.descpool);
  if (ret) {
    return ret;
  }
  vk.delqueue.enqueue(info.descpool, vk.device.device());
  info.vk = vk.vk;
  info.device = vk.device.device();
  info.queue = vk.device.graphics_queue();
  info.swapchain_format = vk.swapchain.format();
  info.physical_device = vk.device.physical_device();
  return ret;
}

} // namespace

fn vk_init_imgui_fn(VkContext_Impl& vk, VkImGuiInitFn imgui_initer) -> VkExpect<void> {
  static constexpr auto pool_sizes =
    std::to_array<VkDescriptorPoolSize>({{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
                                         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                                         {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
                                         {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}});

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 1000;
  pool_info.poolSizeCount = (u32)pool_sizes.size();
  pool_info.pPoolSizes = pool_sizes.data();
  VkImGuiInfo imgui_info;
  if (auto ret = vkmk_imgui_info(vk, imgui_info, pool_info); ret != VK_SUCCESS) {
    return {unexpect, ret};
  }

  ImGui_ImplVulkan_InitInfo init_info{};
  init_info.Instance = imgui_info.vk;
  init_info.PhysicalDevice = imgui_info.physical_device;
  init_info.Device = imgui_info.device;
  init_info.Queue = imgui_info.queue;
  init_info.DescriptorPool = imgui_info.descpool;
  init_info.MinImageCount = 3;
  init_info.ImageCount = 3;
  init_info.UseDynamicRendering = true;

  init_info.PipelineInfoMain.PipelineRenderingCreateInfo.sType =
    VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
  init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats =
    &imgui_info.swapchain_format;
  init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  imgui_initer();
  ImGui_ImplVulkan_Init(&init_info);

  return {};
}

fn vk_draw_imgui(VkCommandBuffer cmd, ImDrawData* draw_data, VkPipeline pipeline) -> void {
  ImGui_ImplVulkan_RenderDrawData(draw_data, cmd, pipeline);
}

fn vk_shutdown_imgui() -> void {
  ImGui_ImplVulkan_Shutdown();
}

} // namespace kappa::render
