#include "./vk_util.hpp"
#include "./vk_private.hpp"

namespace kappa::render {

fn vk_error_string(VkResult res) noexcept -> const char* {
#define VKSTR(code) \
  case code:        \
    return #code;

  switch (res) {
    VKSTR(VK_NOT_READY);
    VKSTR(VK_TIMEOUT);
    VKSTR(VK_EVENT_SET);
    VKSTR(VK_EVENT_RESET);
    VKSTR(VK_INCOMPLETE);
    VKSTR(VK_ERROR_OUT_OF_HOST_MEMORY);
    VKSTR(VK_ERROR_OUT_OF_DEVICE_MEMORY);
    VKSTR(VK_ERROR_INITIALIZATION_FAILED);
    VKSTR(VK_ERROR_DEVICE_LOST);
    VKSTR(VK_ERROR_MEMORY_MAP_FAILED);
    VKSTR(VK_ERROR_LAYER_NOT_PRESENT);
    VKSTR(VK_ERROR_EXTENSION_NOT_PRESENT);
    VKSTR(VK_ERROR_FEATURE_NOT_PRESENT);
    VKSTR(VK_ERROR_INCOMPATIBLE_DRIVER);
    VKSTR(VK_ERROR_TOO_MANY_OBJECTS);
    VKSTR(VK_ERROR_FORMAT_NOT_SUPPORTED);
    VKSTR(VK_ERROR_SURFACE_LOST_KHR);
    VKSTR(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
    VKSTR(VK_SUBOPTIMAL_KHR);
    VKSTR(VK_ERROR_OUT_OF_DATE_KHR);
    VKSTR(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
    VKSTR(VK_ERROR_VALIDATION_FAILED_EXT);
    VKSTR(VK_ERROR_INVALID_SHADER_NV);
    default:
      return "UNKNOWN_ERROR";
  }
#undef VKSTR
};

namespace {

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks* pAllocator) {
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
    instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func) {
    func(instance, debugMessenger, pAllocator);
  }
}

fn handle_name(VulkanDelQueue::HandleType type) -> const char* {
#define STR(_type)                   \
  case VulkanDelQueue::TYPE_##_type: \
    return #_type
  switch (type) {
    STR(IMAGE);
    STR(IMAGE_VIEW);
    STR(SURFACE);
    STR(CMDPOOL);
    STR(FENCE);
    STR(SEMAPHORE);
    STR(VMALLOC);
    STR(MESSENGER);
    STR(DEVICE);
    STR(INSTANCE);
    STR(DESCPOOL);
    STR(DESCLAYOUT);
    STR(PIPLAYOUT);
    STR(PIPELINE);
  }
  KA_UNREACHABLE();
#undef STR
}

} // namespace

fn VulkanDelQueue::flush() -> void {
  static constexpr VkAllocationCallbacks* vkalloc = nullptr;
  for (auto it = _queue.rbegin(); it != _queue.rend(); ++it) {
    const auto [handle, parent, other_parent, type] = *it;
    VK_LOG(verbose, "Deleting {} {}", handle_name(type), fmt::ptr(handle));
    switch (type) {
      case TYPE_IMAGE: {
        vmaDestroyImage((VmaAllocator)other_parent, (VkImage)handle, (VmaAllocation)parent);
        // vkDestroyImage((VkDevice)parent, (VkImage)handle, vkalloc);
      } break;
      case TYPE_IMAGE_VIEW: {
        vkDestroyImageView((VkDevice)parent, (VkImageView)handle, vkalloc);
      } break;
      case TYPE_SURFACE: {
        vkDestroySurfaceKHR((VkInstance)parent, (VkSurfaceKHR)handle, vkalloc);
      } break;
      case TYPE_CMDPOOL: {
        vkDestroyCommandPool((VkDevice)parent, (VkCommandPool)handle, vkalloc);
      } break;
      case TYPE_FENCE: {
        vkDestroyFence((VkDevice)parent, (VkFence)handle, vkalloc);
      } break;
      case TYPE_SEMAPHORE: {
        vkDestroySemaphore((VkDevice)parent, (VkSemaphore)handle, vkalloc);
      } break;
      case TYPE_VMALLOC: {
        vmaDestroyAllocator((VmaAllocator)handle);
      } break;
      case TYPE_MESSENGER: {
        DestroyDebugUtilsMessengerEXT((VkInstance)parent, (VkDebugUtilsMessengerEXT)handle,
                                      vkalloc);
      } break;
      case TYPE_DEVICE: {
        vkDestroyDevice((VkDevice)handle, vkalloc);
      } break;
      case TYPE_INSTANCE: {
        vkDestroyInstance((VkInstance)handle, vkalloc);
      } break;
      case TYPE_DESCPOOL: {
        vkDestroyDescriptorPool((VkDevice)parent, (VkDescriptorPool)handle, vkalloc);
      } break;
      case TYPE_DESCLAYOUT: {
        vkDestroyDescriptorSetLayout((VkDevice)parent, (VkDescriptorSetLayout)handle, vkalloc);
      } break;
      case TYPE_PIPLAYOUT: {
        vkDestroyPipelineLayout((VkDevice)parent, (VkPipelineLayout)handle, vkalloc);
      } break;
      case TYPE_PIPELINE: {
        vkDestroyPipeline((VkDevice)parent, (VkPipeline)handle, vkalloc);
      } break;
    }
  }
  _queue.clear();
}

fn VulkanDelQueue::enqueue_handle(VulkanHandle handle, VulkanHandle parent, HandleType type,
                                  VulkanHandle other_parent) -> void {
  if (handle == VK_NULL_HANDLE) {
    return;
  }
  _queue.emplace_back(handle, parent, other_parent, type);
}

fn vkcmd_transfer_image(VkCommandBuffer cmdbuf, VkImage src, VkImage dst, VkExtent2D src_ext,
                        VkExtent2D dst_ext) -> void {
  // Prepare the region
  VkImageBlit2 blit_region{};
  blit_region.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
  blit_region.pNext = nullptr;

  blit_region.srcOffsets[1].x = src_ext.width;
  blit_region.srcOffsets[1].y = src_ext.height;
  blit_region.srcOffsets[1].z = 1;

  blit_region.dstOffsets[1].x = dst_ext.width;
  blit_region.dstOffsets[1].y = dst_ext.height;
  blit_region.dstOffsets[1].z = 1;

  blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  blit_region.srcSubresource.baseArrayLayer = 0;
  blit_region.srcSubresource.layerCount = 1;
  blit_region.srcSubresource.mipLevel = 0;

  blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  blit_region.dstSubresource.baseArrayLayer = 0;
  blit_region.dstSubresource.layerCount = 1;
  blit_region.dstSubresource.mipLevel = 0;

  // Blit the thing
  VkBlitImageInfo2 blit_info{};
  blit_info.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
  blit_info.pNext = nullptr;
  blit_info.dstImage = dst;
  blit_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  blit_info.srcImage = src;
  blit_info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  blit_info.filter = VK_FILTER_LINEAR;
  blit_info.pRegions = &blit_region;
  blit_info.regionCount = 1;
  vkCmdBlitImage2(cmdbuf, &blit_info);
}

fn vkcmd_transition_image(VkCommandBuffer cmd, VkImage img, VkImageLayout curr_layout,
                          VkImageLayout new_layout) -> void {
  VkImageMemoryBarrier2 barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier.pNext = nullptr;

  barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
  barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

  barrier.oldLayout = curr_layout;
  barrier.newLayout = new_layout;

  VkImageAspectFlags aspect_mask = (new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
                                   ? VK_IMAGE_ASPECT_DEPTH_BIT
                                   : VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange = vkmk_image_subresource_range(aspect_mask);
  barrier.image = img;

  VkDependencyInfo dep_info{};
  dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dep_info.pNext = nullptr;

  dep_info.imageMemoryBarrierCount = 1;
  dep_info.pImageMemoryBarriers = &barrier;

  vkCmdPipelineBarrier2(cmd, &dep_info);
}

fn vkmk_semaphore_submit_info(VkPipelineStageFlags2 mask, VkSemaphore sem)
  -> VkSemaphoreSubmitInfo {
  VkSemaphoreSubmitInfo submit{};
  submit.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  submit.pNext = nullptr;
  submit.semaphore = sem;
  submit.stageMask = mask;
  submit.deviceIndex = 0;
  submit.value = 1;
  return submit;
}

fn vkmk_command_buffer_submit_info(VkCommandBuffer cmdbuf) -> VkCommandBufferSubmitInfo {
  VkCommandBufferSubmitInfo info{};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
  info.pNext = nullptr;
  info.commandBuffer = cmdbuf;
  info.deviceMask = 0;
  return info;
}

fn vkmk_submit_info(const VkCommandBufferSubmitInfo& cmd_info,
                    const VkSemaphoreSubmitInfo* signal_sem_info,
                    const VkSemaphoreSubmitInfo* wait_sem_info) -> VkSubmitInfo2 {
  VkSubmitInfo2 info{};
  info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
  info.pNext = nullptr;

  info.waitSemaphoreInfoCount = wait_sem_info == nullptr ? 0 : 1;
  info.pWaitSemaphoreInfos = wait_sem_info;

  info.signalSemaphoreInfoCount = signal_sem_info == nullptr ? 0 : 1;
  info.pSignalSemaphoreInfos = signal_sem_info;

  info.commandBufferInfoCount = 1;
  info.pCommandBufferInfos = &cmd_info;

  return info;
}

fn vkmk_image_info(VkFormat format, VkImageUsageFlags usage, VkExtent3D extent)
  -> VkImageCreateInfo {
  VkImageCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  info.pNext = nullptr;

  info.imageType = VK_IMAGE_TYPE_2D;

  info.format = format;
  info.extent = extent;

  info.mipLevels = 1;
  info.arrayLayers = 1;

  // for MSAA. we will not be using it by default, so default it to 1 sample per pixel.
  info.samples = VK_SAMPLE_COUNT_1_BIT;

  // optimal tiling, which means the image is stored on the best gpu format
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = usage;

  return info;
}

fn vkmk_imageview_info(VkFormat format, VkImage image, VkImageAspectFlags aspect_mask)
  -> VkImageViewCreateInfo {
  // build a image-view for the depth image to use for rendering
  VkImageViewCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  info.pNext = nullptr;

  // map the color channels to something, vk_component_swizzle_identity by default
  info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

  // Only one layer per image
  info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  info.image = image;
  info.format = format;
  info.subresourceRange.baseMipLevel = 0;
  info.subresourceRange.levelCount = 1;
  info.subresourceRange.baseArrayLayer = 0;
  info.subresourceRange.layerCount = 1;
  info.subresourceRange.aspectMask = aspect_mask;

  return info;
}

fn vkmk_image_subresource_range(VkImageAspectFlags mask) -> VkImageSubresourceRange {
  VkImageSubresourceRange sub_image{};
  sub_image.aspectMask = mask;
  sub_image.baseMipLevel = 0;
  sub_image.levelCount = VK_REMAINING_MIP_LEVELS;
  sub_image.baseArrayLayer = 0;
  sub_image.layerCount = VK_REMAINING_ARRAY_LAYERS;
  return sub_image;
};

fn vkmk_fence_info(VkFenceCreateFlags flags) -> VkFenceCreateInfo {
  VkFenceCreateInfo fence_create_info{};
  fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_create_info.pNext = nullptr;
  fence_create_info.flags = flags;
  return fence_create_info;
}

fn vkmk_semaphore_info(VkSemaphoreCreateFlags flags) -> VkSemaphoreCreateInfo {
  VkSemaphoreCreateInfo semaphore_create_info{};
  semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphore_create_info.pNext = nullptr;
  semaphore_create_info.flags = flags;
  return semaphore_create_info;
}

fn vkmk_cmdbuf_begin_info(VkCommandBufferUsageFlags flags) -> VkCommandBufferBeginInfo {
  VkCommandBufferBeginInfo cmd_begin_info{};
  cmd_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  cmd_begin_info.pNext = nullptr;
  cmd_begin_info.pInheritanceInfo = nullptr;
  cmd_begin_info.flags = flags;
  return cmd_begin_info;
}

fn vkmk_cmdpool_info(VkCommandPoolCreateFlags flags, u32 family_index) -> VkCommandPoolCreateInfo {
  VkCommandPoolCreateInfo cmdpool_info{};
  cmdpool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  cmdpool_info.pNext = nullptr;
  cmdpool_info.flags = flags;
  cmdpool_info.queueFamilyIndex = family_index;
  return cmdpool_info;
}

fn vkmk_cmdbuf_alloc_info(VkCommandPool cmdpool, VkCommandBufferLevel level)
  -> VkCommandBufferAllocateInfo {
  VkCommandBufferAllocateInfo cmd_alloc_info{};
  cmd_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cmd_alloc_info.pNext = nullptr;
  cmd_alloc_info.commandPool = cmdpool;
  cmd_alloc_info.commandBufferCount = 1;
  cmd_alloc_info.level = level;
  return cmd_alloc_info;
}

// Framebufer creation, old
#if 0
fn make_fbo() {
  // Specify all the framebuffer attachments that will be used while rendering

  // A single color buffer attachment, represented by one of the images from the swap chain
  VkAttachmentDescription color_attachment{};
  color_attachment.format = image_format; // Same as the swapchain image format
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;

  // What will happen with the data in the attachment before and after rendering
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;   // Clear values at the start
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Store rendered contents in memory

  // No stencil buffer for now
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

  // Layout for the framebuffer image (?) before and after the render pass
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  // Things for subpasses, each one references one or more of the previous attachments
  VkAttachmentReference color_attachment_ref{};
  color_attachment_ref.attachment = 0;
  color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  // Only one subpass for the fragment shader out_color
  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // It's a graphics subpass
  subpass.colorAttachmentCount = 1;

  // This maps to the location index in the shader:
  // layout(location = 0) out vec4 out_color;
  subpass.pColorAttachments = &color_attachment_ref;

  VkSubpassDependency dep{};
  dep.srcSubpass = VK_SUBPASS_EXTERNAL;
  dep.dstSubpass = 0;
  dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dep.srcAccessMask = 0;
  dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo render_pass_info{};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = 1;
  render_pass_info.pAttachments = &color_attachment;
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass;
  render_pass_info.dependencyCount = 1;
  render_pass_info.pDependencies = &dep;

  VkRenderPass renderpass;
  res = vkCreateRenderPass(device, &render_pass_info, vkalloc, &renderpass);
  if (res != VK_SUCCESS) {
    for (auto& view : swapchain_image_views) {
      vkDestroyImageView(device, view, vkalloc);
    }
    vkDestroySwapchainKHR(device, swapchain, vkalloc);
    return {ntf::unexpect, "Failed to create render pass", res};
  }

  std::vector<VkFramebuffer> framebuffers;
  framebuffers.resize(swapchain_image_views.size());
  for (u32 i = 0; i < swapchain_image_views.size(); ++i) {
    VkImageView attachments[] = {swapchain_image_views[i]};

    VkFramebufferCreateInfo framebuffer{};
    framebuffer.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer.renderPass = renderpass;
    framebuffer.attachmentCount = 1;
    framebuffer.pAttachments = attachments;
    framebuffer.width = swapchain_extent.width;
    framebuffer.height = swapchain_extent.height;
    framebuffer.layers = 1; // Layers in the image arrays

    VK_ASSERT(vkCreateFramebuffer(device, &framebuffer, vkalloc, &framebuffers[i]));
  }
}
#endif

} // namespace kappa::render
