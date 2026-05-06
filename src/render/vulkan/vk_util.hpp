#pragma once

#include "./vk_private.hpp"

#include "../../util/array.hpp"
#include <vulkan/vulkan_core.h>

#define KA_VK_STRUCT(_typename, _sType)                                  \
  template<>                                                             \
  struct VulkanStructTraits<_typename> {                                 \
    static constexpr bool is_specialized = true;                         \
    static constexpr VkStructureType sType = VK_STRUCTURE_TYPE_##_sType; \
  }

namespace kappa::render {

template<typename T>
struct VulkanStructTraits {
  static constexpr bool is_specialized = false;
};

KA_VK_STRUCT(VkApplicationInfo, APPLICATION_INFO);
KA_VK_STRUCT(VkInstanceCreateInfo, INSTANCE_CREATE_INFO);
KA_VK_STRUCT(VkDebugUtilsMessengerCreateInfoEXT, DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT);
KA_VK_STRUCT(VkPipelineShaderStageCreateInfo, PIPELINE_SHADER_STAGE_CREATE_INFO);
KA_VK_STRUCT(VkComputePipelineCreateInfo, COMPUTE_PIPELINE_CREATE_INFO);
KA_VK_STRUCT(VkPresentInfoKHR, PRESENT_INFO_KHR);
KA_VK_STRUCT(VkImageBlit2, IMAGE_BLIT_2);
KA_VK_STRUCT(VkBlitImageInfo2, BLIT_IMAGE_INFO_2);
KA_VK_STRUCT(VkImageMemoryBarrier2, IMAGE_MEMORY_BARRIER_2);
KA_VK_STRUCT(VkDependencyInfo, DEPENDENCY_INFO);
KA_VK_STRUCT(VkSemaphoreSubmitInfo, SEMAPHORE_SUBMIT_INFO);
KA_VK_STRUCT(VkSubmitInfo2, SUBMIT_INFO_2);
KA_VK_STRUCT(VkCommandBufferSubmitInfo, COMMAND_BUFFER_SUBMIT_INFO);
KA_VK_STRUCT(VkImageCreateInfo, IMAGE_CREATE_INFO);
KA_VK_STRUCT(VkImageViewCreateInfo, IMAGE_VIEW_CREATE_INFO);
KA_VK_STRUCT(VkFenceCreateInfo, FENCE_CREATE_INFO);
KA_VK_STRUCT(VkSemaphoreCreateInfo, SEMAPHORE_CREATE_INFO);
KA_VK_STRUCT(VkCommandBufferBeginInfo, COMMAND_BUFFER_BEGIN_INFO);
KA_VK_STRUCT(VkCommandPoolCreateInfo, COMMAND_POOL_CREATE_INFO);
KA_VK_STRUCT(VkCommandBufferAllocateInfo, COMMAND_BUFFER_ALLOCATE_INFO);
KA_VK_STRUCT(VkRenderingInfo, RENDERING_INFO);
KA_VK_STRUCT(VkRenderingAttachmentInfo, RENDERING_ATTACHMENT_INFO);
KA_VK_STRUCT(VkDescriptorSetAllocateInfo, DESCRIPTOR_SET_ALLOCATE_INFO);
KA_VK_STRUCT(VkShaderModuleCreateInfo, SHADER_MODULE_CREATE_INFO);
KA_VK_STRUCT(VkDescriptorSetLayoutCreateInfo, DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
KA_VK_STRUCT(VkDescriptorPoolCreateInfo, DESCRIPTOR_POOL_CREATE_INFO);
KA_VK_STRUCT(VkDeviceQueueCreateInfo, DEVICE_QUEUE_CREATE_INFO);
KA_VK_STRUCT(VkPhysicalDeviceVulkan13Features, PHYSICAL_DEVICE_VULKAN_1_3_FEATURES);
KA_VK_STRUCT(VkPhysicalDeviceVulkan12Features, PHYSICAL_DEVICE_VULKAN_1_2_FEATURES);
KA_VK_STRUCT(VkDeviceCreateInfo, DEVICE_CREATE_INFO);
KA_VK_STRUCT(VkSwapchainCreateInfoKHR, SWAPCHAIN_CREATE_INFO_KHR);
KA_VK_STRUCT(VkPipelineLayoutCreateInfo, PIPELINE_LAYOUT_CREATE_INFO);
KA_VK_STRUCT(VkWriteDescriptorSet, WRITE_DESCRIPTOR_SET);

template<typename T>
requires(VulkanStructTraits<T>::is_specialized)
fn vkmk_zero(void* pNext = nullptr) -> T {
  T info;
  std::memset(&info, 0x00, sizeof(info));
  info.sType = VulkanStructTraits<T>::sType;
  info.pNext = pNext;
  return info;
}

template<typename T>
fn vkmk_zero(VkStructureType sType, void* pNext = nullptr) -> T {
  T info;
  std::memset(&info, 0x00, sizeof(info));
  info.sType = sType;
  info.pNext = pNext;
  return info;
}

class VulkanDelQueue {
public:
  using VulkanHandle = void*;
  using DelFn = TrivFn<void(), 3 * sizeof(VulkanHandle), 8>;

  enum HandleType {
    TYPE_DELETER,
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

  struct HandleSet {
    VulkanHandle handle;
    VulkanHandle parent;
    VulkanHandle other_parent;
  };

  struct DelData {
    DelData(HandleType type, VulkanHandle handle, VulkanHandle parent, VulkanHandle other_parent) :
        handles(handle, parent, other_parent), type(type) {}

    DelData(DelFn&& deleter_) : deleter(std::move(deleter_)), type(TYPE_DELETER) {}

    union {
      HandleSet handles;
      DelFn deleter;
    };

    HandleType type;
  };

public:
  VulkanDelQueue() = default;

public:
  fn enqueue_deleter(DelFn func) -> void;
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

fn vkmk_render_info(VkExtent2D render_extent, const VkRenderingAttachmentInfo* color_attachment,
                    const VkRenderingAttachmentInfo* depth_attachment) -> VkRenderingInfo;

fn vkmk_attach_info(VkImageView view, VkClearValue* clear, VkImageLayout layout)
  -> VkRenderingAttachmentInfo;

} // namespace kappa::render
