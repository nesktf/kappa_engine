#pragma once

#include "./vk_private.hpp"

#include "./vk_buffer.hpp"
#include "./vk_device.hpp"
#include "./vk_pipeline.hpp"
#include "./vk_swapchain.hpp"
#include "./vk_util.hpp"

namespace kappa::render {

struct VkContext_Impl {
public:
  struct ImDrawData {
    VkFence fence;
    VkCommandPool cmdpool;
    VkCommandBuffer cmdbuf;
  };

public:
  VkContext_Impl(VkInstance vk_, VkDebugUtilsMessengerEXT messenger_, VmaAllocator vmalloc_,
                 VkSurfaceKHR surface_, kappa::render::VulkanDevice&& device_,
                 kappa::render::VulkanSwapchain&& swapchain_,
                 kappa::render::VulkanFrameData&& framedata_, ImDrawData&& imdrawdata_,
                 kappa::render::VulkanDescPool&& descpool_,
                 kappa::render::VulkanDelQueue&& delqueue_);
  ~VkContext_Impl();
  KA_NO_MOVE(VkContext_Impl);
  KA_NO_COPY(VkContext_Impl);

public:
  VkInstance vk;
  VkDebugUtilsMessengerEXT messenger;
  VmaAllocator vmalloc;
  VkSurfaceKHR surface;
  ImDrawData imdrawdata;
  kappa::render::VulkanDevice device;
  kappa::render::VulkanSwapchain swapchain;
  kappa::render::VulkanFrameData framedata;
  kappa::render::VulkanDescPool descpool;
  kappa::render::VulkanDelQueue delqueue;
};

} // namespace kappa::render

#if 0
namespace kappa::render {

constexpr usize EFFECT_COUNT = 2;

struct GPUMeshBuffers {
  VulkanBuffer index_buffer;
  VulkanBuffer vertex_buffer;
  VkDeviceAddress vertex_buffer_addr;
  u32 index_count;
};

struct VulkanContext_Impl {
public:
  struct ImDrawData {
    VkFence fence;
    VkCommandPool cmdpool;
    VkCommandBuffer cmdbuf;
  };

  // temp
  struct DrawThing {
    VulkanImage image;
    VkExtent2D extent;
    VulkanDescPool desc_pool;
    VkDescriptorSet image_desc;
    VkDescriptorSetLayout image_desc_layout;
    std::array<ComputeEffect, EFFECT_COUNT> background_effects;
    s32 effect_idx;
    VkPipeline triangle_pipeline;
    VkPipelineLayout triangle_layout;
    GPUMeshBuffers mesh_buffers;
    VkPipeline mesh_pipeline;
    VkPipelineLayout mesh_layout;
  };

public:
  VulkanContext_Impl() = default; // Manually initialized
  ~VulkanContext_Impl() = default;
  KA_NO_MOVE(VulkanContext_Impl);
  KA_NO_COPY(VulkanContext_Impl);

public:
};

fn vk_get_graphics_queue(VulkanContext_Impl& ctx) -> VkQueue;

} // namespace kappa::render
#endif
