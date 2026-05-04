#include "./vk_private.hpp"

#include "./vk_context.hpp"

#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan_core.h>

namespace kappa::render {

fn vk_init_imgui(VulkanContext& ctx_, ImGuiFn imgui_init) -> void {
  auto* ctx = ctx_.get();
  // 1: create descriptor pool for IMGUI
  //  the size of the pool is very oversize, but it's copied from imgui demo
  //  itself.
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

  const auto device = ctx->device->device();

  VkDescriptorPool imgui_pool;
  VK_ASSERT(vkCreateDescriptorPool(device, &pool_info, vkalloc, &imgui_pool));

  imgui_init();

  const auto swapchain_format = ctx->swapchain->format();

  ImGui_ImplVulkan_InitInfo init_info{};
  init_info.Instance = ctx->vk;
  init_info.PhysicalDevice = ctx->device->physical_device();
  init_info.Device = ctx->device->device();
  init_info.Queue = vk_get_graphics_queue(*ctx);
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

  ctx->delqueue.enqueue_deleter([=]() {
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

fn vk_imgui_frame(VulkanContext&, ImGuiFn draw) -> void {
  ImGui_ImplVulkan_NewFrame();
  draw();
}

} // namespace kappa::render
