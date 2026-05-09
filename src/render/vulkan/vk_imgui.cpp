#include "./vk_imgui.hpp"

#include "./vk_device.hpp"
#include "./vk_swapchain.hpp"

#include <imgui.h>
#include <imgui_impl_vulkan.h>

namespace kappa::render {

fn vk_init_imgui(VkInstance vk, const VulkanDevice& dev, const VulkanSwapchain& swapchain,
                 VulkanDelQueue& delqueue, PFN_ka_vk_init_imgui init_imgui, void* init_imgui_user)
  -> void {
  ka_assert(init_imgui);

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

  const auto device = dev.device();

  VkDescriptorPool imgui_pool;
  KA_VK_ASSERT(vkCreateDescriptorPool(device, &pool_info, vkalloc, &imgui_pool));

  init_imgui(init_imgui_user);

  const auto swapchain_format = swapchain.format();

  ImGui_ImplVulkan_InitInfo init_info{};
  init_info.Instance = vk;
  init_info.PhysicalDevice = dev.physical_device();
  init_info.Device = dev.device();
  init_info.Queue = dev.graphics_queue();
  init_info.DescriptorPool = imgui_pool;
  init_info.MinImageCount = 3;
  init_info.ImageCount = 3;
  init_info.UseDynamicRendering = true;

  init_info.PipelineInfoMain.PipelineRenderingCreateInfo.sType =
    VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
  init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats =
    &swapchain_format;
  init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  ImGui_ImplVulkan_Init(&init_info);
  // ImGui_ImplVulkan_CreateFontsTexture();

  delqueue.enqueue_deleter([=]() {
    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(device, imgui_pool, vkalloc);
  });
}

fn vk_draw_imgui(VkCommandBuffer cmd, VkImageView target, VkExtent2D swapchain_extent) -> void {
  const auto color_attachment =
    vkmk_attach_info(target, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  const auto render_info = vkmk_render_info(swapchain_extent, &color_attachment, nullptr);

  vkCmdBeginRendering(cmd, &render_info);
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
  vkCmdEndRendering(cmd);
}

} // namespace kappa::render
