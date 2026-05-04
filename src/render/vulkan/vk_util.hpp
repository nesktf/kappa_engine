#pragma once

#include "./vk_private.hpp"

#include "../../util/array.hpp"

namespace kappa::render {

class VulkanDelQueue {
public:
  using VulkanHandle = void*;

  enum HandleType {
    TYPE_IMAGE,
    TYPE_IMAGE_VIEW,
    TYPE_SURFACE,
    TYPE_CMDPOOL,
    TYPE_FENCE,
    TYPE_SEMAPHORE,
    TYPE_VMALLOC,
    TYPE_MESSENGER,
    TYPE_DEVICE,
    TYPE_INSTANCE,
    TYPE_DESCPOOL,
    TYPE_DESCLAYOUT,
    TYPE_PIPLAYOUT,
    TYPE_PIPELINE,
  };

  struct DelData {
    VulkanHandle handle;
    VulkanHandle parent;
    VulkanHandle other_parent;
    HandleType type;
  };

public:
  VulkanDelQueue() = default;

public:
  fn enqueue_handle(VulkanHandle handle, VulkanHandle parent, HandleType type,
                    VulkanHandle other_parent = VK_NULL_HANDLE) -> void;
  fn flush() -> void;

  fn enqueue(VkImage image, VmaAllocation allocation, VmaAllocator alloc) -> void {
    enqueue_handle((VulkanHandle)image, (VulkanHandle)allocation, TYPE_IMAGE, (VulkanHandle)alloc);
  }

  fn enqueue(VkImageView image_view, VkDevice device) -> void {
    enqueue_handle((VulkanHandle)image_view, (VulkanHandle)device, TYPE_IMAGE_VIEW);
  }

  fn enqueue(VkSurfaceKHR surface, VkInstance instance) -> void {
    enqueue_handle((VulkanHandle)surface, (VulkanHandle)instance, TYPE_SURFACE);
  }

  fn enqueue(VkCommandPool pool, VkDevice device) -> void {
    enqueue_handle((VulkanHandle)pool, (VulkanHandle)device, TYPE_CMDPOOL);
  }

  fn enqueue(VkFence fence, VkDevice device) -> void {
    enqueue_handle((VulkanHandle)fence, (VulkanHandle)device, TYPE_FENCE);
  }

  fn enqueue(VkSemaphore semaphore, VkDevice device) -> void {
    enqueue_handle((VulkanHandle)semaphore, (VulkanHandle)device, TYPE_SEMAPHORE);
  }

  fn enqueue(VmaAllocator alloc, VkDevice device) -> void {
    enqueue_handle((VulkanHandle)alloc, (VulkanHandle)device, TYPE_VMALLOC);
  }

  fn enqueue(VkDebugUtilsMessengerEXT messenger, VkInstance instance) -> void {
    enqueue_handle((VulkanHandle)messenger, (VulkanHandle)instance, TYPE_MESSENGER);
  }

  fn enqueue(VkDevice device) -> void {
    enqueue_handle((VulkanHandle)device, VK_NULL_HANDLE, TYPE_DEVICE);
  }

  fn enqueue(VkInstance instance) -> void {
    enqueue_handle((VulkanHandle)instance, VK_NULL_HANDLE, TYPE_INSTANCE);
  }

  fn enqueue(VkDescriptorPool pool, VkDevice device) -> void {
    enqueue_handle((VulkanHandle)pool, (VulkanHandle)device, TYPE_DESCPOOL);
  }

  fn enqueue(VkDescriptorSetLayout layout, VkDevice device) -> void {
    enqueue_handle((VulkanHandle)layout, (VulkanHandle)device, TYPE_DESCLAYOUT);
  }

  fn enqueue(VkPipelineLayout layout, VkDevice device) -> void {
    enqueue_handle((VulkanHandle)layout, (VulkanHandle)device, TYPE_PIPLAYOUT);
  }

  fn enqueue(VkPipeline pipeline, VkDevice device) -> void {
    enqueue_handle((VulkanHandle)pipeline, (VulkanHandle)device, TYPE_PIPELINE);
  }

private:
  Vec<DelData> _queue;
};

fn vkcmd_transfer_image(VkCommandBuffer cmdbuf, VkImage src, VkImage dst, VkExtent2D src_ext,
                        VkExtent2D dst_ext) -> void;

fn vkcmd_transition_image(VkCommandBuffer cmd, VkImage img, VkImageLayout curr_layout,
                          VkImageLayout new_layout) -> void;

fn vkmk_semaphore_info(VkSemaphoreCreateFlags flags) -> VkSemaphoreCreateInfo;

fn vkmk_fence_info(VkFenceCreateFlags flags) -> VkFenceCreateInfo;

fn vkmk_semaphore_submit_info(VkPipelineStageFlags2 mask, VkSemaphore sem)
  -> VkSemaphoreSubmitInfo;

fn vkmk_command_buffer_submit_info(VkCommandBuffer cmdbuf) -> VkCommandBufferSubmitInfo;

fn vkmk_submit_info(const VkCommandBufferSubmitInfo& cmd_info,
                    const VkSemaphoreSubmitInfo* signal_sem_info,
                    const VkSemaphoreSubmitInfo* wait_sem_info) -> VkSubmitInfo2;

fn vkmk_image_info(VkFormat format, VkImageUsageFlags usage, VkExtent3D extent)
  -> VkImageCreateInfo;

fn vkmk_imageview_info(VkFormat format, VkImage image, VkImageAspectFlags aspect_mask)
  -> VkImageViewCreateInfo;

fn vkmk_image_subresource_range(VkImageAspectFlags mask) -> VkImageSubresourceRange;

fn vkmk_cmdbuf_begin_info(VkCommandBufferUsageFlags flags) -> VkCommandBufferBeginInfo;

fn vkmk_cmdpool_info(VkCommandPoolCreateFlags flags, u32 family_index) -> VkCommandPoolCreateInfo;

fn vkmk_cmdbuf_alloc_info(VkCommandPool cmdpool, VkCommandBufferLevel level)
  -> VkCommandBufferAllocateInfo;

} // namespace kappa::render
