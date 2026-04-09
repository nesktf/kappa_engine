#pragma once

#include "../../core.hpp"

#include <vulkan/vulkan_core.h>

namespace kappa::render {

fn vk_transfer_image(VkCommandBuffer cmdbuf, VkImage src, VkImage dst, VkExtent2D src_ext,
                     VkExtent2D dst_ext) -> void;

fn vk_transition_image(VkCommandBuffer cmd, VkImage img, VkImageLayout curr_layout,
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
