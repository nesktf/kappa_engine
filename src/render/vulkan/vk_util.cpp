#include "./vk_util.hpp"

#include "./vk_private.hpp"

#include "./vk_buffer.hpp"
#include "./vk_image.hpp"

namespace kappa::render {

namespace {

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks* pAllocator) {
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
    instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func) {
    func(instance, debugMessenger, pAllocator);
  }
}

fn handle_name(VkDelQueue::HandleType type) -> const char* {
#define STR(_type)               \
  case VkDelQueue::TYPE_##_type: \
    return #_type
  switch (type) {
    STR(DELETER);
    STR(BUFFER);
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
    STR(SHADER);
    STR(SAMPLER);
    default:
      return "UNKNOWN";
  }
#undef STR
}

fn destroy_handle(VkDelQueue::HandleType type, VkHandle parent, VkHandle other_parent,
                  VkHandle handle) -> void {
  using enum VkDelQueue::HandleType;
  KA_VK_LOG(verbose, "Deleting {} {}", handle_name(type), fmt::ptr(handle));
  switch (type) {
    case TYPE_IMAGE: {
      vmaDestroyImage((VmaAllocator)other_parent, (VkImage)handle, (VmaAllocation)parent);
    } break;
    case TYPE_BUFFER: {
      vmaDestroyBuffer((VmaAllocator)other_parent, (VkBuffer)handle, (VmaAllocation)parent);
    } break;
    case TYPE_SAMPLER: {
      vkDestroySampler((VkDevice)parent, (VkSampler)handle, vkalloc);
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
      DestroyDebugUtilsMessengerEXT((VkInstance)parent, (VkDebugUtilsMessengerEXT)handle, vkalloc);
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
    case TYPE_SHADER: {
      vkDestroyShaderModule((VkDevice)parent, (VkShaderModule)handle, vkalloc);
    } break;
    case TYPE_DELETER:
      break;
  }
}

} // namespace

fn VkDelQueue::flush() -> void {
  for (auto it = _queue.rbegin(); it != _queue.rend(); ++it) {
    auto& deldata = *it;
    if (deldata.type == TYPE_DELETER) {
      deldata.deleter();
    } else {
      destroy_handle(deldata.type, deldata.handles.parent, deldata.handles.other_parent,
                     deldata.handles.handle);
    }
  }
  _queue.clear();
}

fn VkDelQueue::enqueue(VkAllocImage& image, VkDevice device, VkMemAllocator vma) -> void {
  enqueue(image.view(), device);
  enqueue_handle((VkHandle)image.image(), (VkHandle)image.allocation(), TYPE_IMAGE, (VkHandle)vma);
}

fn VkDelQueue::enqueue(VkAllocBuff& buffer, VkMemAllocator vma) -> void {
  enqueue_handle((VkHandle)buffer.buffer(), (VkHandle)buffer.allocation(), TYPE_BUFFER,
                 (VkHandle)vma);
}

fn VkDelQueue::enqueue_deleter(DelFn func) -> void {
  _queue.emplace_back(std::move(func));
}

fn VkDelQueue::enqueue_handle(VkHandle handle, VkHandle parent, HandleType type,
                              VkHandle other_parent) -> void {
  if (handle == VK_NULL_HANDLE) {
    return;
  }
  _queue.emplace_back(type, handle, parent, other_parent);
}

fn vkcmd_transfer_image(VkCommandBuffer cmdbuf, VkImage src, VkImage dst, VkExtent2D src_ext,
                        VkExtent2D dst_ext) -> void {
  // Prepare the region
  auto blit_region = vkmk_zero<VkImageBlit2>();
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
  auto blit_info = vkmk_zero<VkBlitImageInfo2>();
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
                          VkImageLayout new_layout) -> VkImageLayout {
  auto barrier = vkmk_zero<VkImageMemoryBarrier2>();
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

  auto dep_info = vkmk_zero<VkDependencyInfo>();
  dep_info.imageMemoryBarrierCount = 1;
  dep_info.pImageMemoryBarriers = &barrier;

  vkCmdPipelineBarrier2(cmd, &dep_info);
  return new_layout;
}

fn vkmk_semaphore_submit_info(VkPipelineStageFlags2 mask, VkSemaphore sem)
  -> VkSemaphoreSubmitInfo {
  auto submit = vkmk_zero<VkSemaphoreSubmitInfo>();
  submit.semaphore = sem;
  submit.stageMask = mask;
  submit.deviceIndex = 0;
  submit.value = 1;
  return submit;
}

fn vkmk_command_buffer_submit_info(VkCommandBuffer cmdbuf) -> VkCommandBufferSubmitInfo {
  auto info = vkmk_zero<VkCommandBufferSubmitInfo>();
  info.commandBuffer = cmdbuf;
  info.deviceMask = 0;
  return info;
}

fn vkmk_submit_info(const VkCommandBufferSubmitInfo& cmd_info,
                    const VkSemaphoreSubmitInfo* signal_sem_info,
                    const VkSemaphoreSubmitInfo* wait_sem_info) -> VkSubmitInfo2 {
  auto info = vkmk_zero<VkSubmitInfo2>();
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
  auto info = vkmk_zero<VkImageCreateInfo>();
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
  auto info = vkmk_zero<VkImageViewCreateInfo>();

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
  auto fence_create_info = vkmk_zero<VkFenceCreateInfo>();
  fence_create_info.flags = flags;
  return fence_create_info;
}

fn vkmk_semaphore_info(VkSemaphoreCreateFlags flags) -> VkSemaphoreCreateInfo {
  auto semaphore_create_info = vkmk_zero<VkSemaphoreCreateInfo>();
  semaphore_create_info.flags = flags;
  return semaphore_create_info;
}

fn vkmk_cmdbuf_begin_info(VkCommandBufferUsageFlags flags) -> VkCommandBufferBeginInfo {
  auto cmd_begin_info = vkmk_zero<VkCommandBufferBeginInfo>();
  cmd_begin_info.pInheritanceInfo = nullptr;
  cmd_begin_info.flags = flags;
  return cmd_begin_info;
}

fn vkmk_cmdpool_info(VkCommandPoolCreateFlags flags, u32 family_index) -> VkCommandPoolCreateInfo {
  auto cmdpool_info = vkmk_zero<VkCommandPoolCreateInfo>();
  cmdpool_info.flags = flags;
  cmdpool_info.queueFamilyIndex = family_index;
  return cmdpool_info;
}

fn vkmk_cmdbuf_alloc_info(VkCommandPool cmdpool, VkCommandBufferLevel level)
  -> VkCommandBufferAllocateInfo {
  auto cmd_alloc_info = vkmk_zero<VkCommandBufferAllocateInfo>();
  cmd_alloc_info.commandPool = cmdpool;
  cmd_alloc_info.commandBufferCount = 1;
  cmd_alloc_info.level = level;
  return cmd_alloc_info;
}

fn vkmk_render_info(VkExtent2D render_extent, const VkRenderingAttachmentInfo* color_attachment,
                    const VkRenderingAttachmentInfo* depth_attachment) -> VkRenderingInfo {
  auto info = vkmk_zero<VkRenderingInfo>();
  info.renderArea.offset.x = 0;
  info.renderArea.offset.y = 0;
  info.renderArea.extent = render_extent;

  info.layerCount = 1;
  info.colorAttachmentCount = 1;
  info.pColorAttachments = color_attachment;
  info.pDepthAttachment = depth_attachment;
  info.pStencilAttachment = nullptr;

  return info;
}

fn vkmk_attach_info(VkImageView view, VkClearValue* clear, VkImageLayout layout)
  -> VkRenderingAttachmentInfo {
  auto info = vkmk_zero<VkRenderingAttachmentInfo>();
  info.imageView = view;
  info.imageLayout = layout;
  info.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
  info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  if (clear) {
    info.clearValue = *clear;
  }

  return info;
}

fn vkmk_depth_attach_info(VkImageView view, VkImageLayout layout) -> VkRenderingAttachmentInfo {
  auto info = vkmk_zero<VkRenderingAttachmentInfo>();
  info.imageView = view;
  info.imageLayout = layout;
  info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  info.clearValue.depthStencil.depth = 0.f;
  return info;
}

fn vkmk_pipeline_stage_info(VkShaderStageFlagBits usage, VkShaderModule shader,
                            const char* entrypoint) -> VkPipelineShaderStageCreateInfo {
  auto info = vkmk_zero<VkPipelineShaderStageCreateInfo>();
  info.stage = usage;
  info.module = shader;
  info.pName = entrypoint ? entrypoint : "main";
  return info;
}

fn vkmk_pipeline_layout_info() -> VkPipelineLayoutCreateInfo {
  auto info = vkmk_zero<VkPipelineLayoutCreateInfo>();
  info.flags = 0;
  info.setLayoutCount = 0;
  info.pSetLayouts = nullptr;
  info.pushConstantRangeCount = 0;
  info.pPushConstantRanges = nullptr;
  return info;
}

} // namespace kappa::render
