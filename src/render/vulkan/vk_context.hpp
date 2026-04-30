#pragma once

#include "./vk_private.hpp"

#include "./vk_buffer.hpp"
#include "./vk_device.hpp"
#include "./vk_swapchain.hpp"
#include "./vk_util.hpp"

namespace kappa::render {

struct VulkanContextImpl {
  struct FrameData {
    VkCommandPool cmdpool;
    VkCommandBuffer cmdbuf;
    VkSemaphore swapchain_sem, render_sem;
    VkFence render_fen;
  };

  // temp
  struct DrawThing {
    VulkanImage image;
    VkExtent2D extent;
    VkDescriptorPool desc_pool;
    VkDescriptorSet image_desc;
    VkDescriptorSetLayout image_desc_layout;
    VkPipeline gradient_pipeline;
    VkPipelineLayout gradient_layout;
  };

  VkInstance vk;
  VkDebugUtilsMessengerEXT messenger;
  VmaAllocator vmalloc;
  VulkanDevice device;
  VkSurfaceKHR surface;
  VulkanSwapchain swapchain;
  VulkanDelQueue delqueue;

  FrameData frames[MAX_FRAMES_IN_FLIGHT];
  u32 curr_frame;

  VkDescriptorPool desc_alloc;
  DrawThing draw;
};

} // namespace kappa::render
