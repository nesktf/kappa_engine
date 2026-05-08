#pragma once

#include "./vk_private.hpp"

#include "./vk_buffer.hpp"
#include "./vk_device.hpp"
#include "./vk_pipeline.hpp"
#include "./vk_swapchain.hpp"
#include "./vk_util.hpp"

#include "../../util/buffer.hpp"

namespace kappa::render {

constexpr usize EFFECT_COUNT = 2;

struct GPUMeshBuffers {
  VulkanBuffer index_buffer;
  VulkanBuffer vertex_buffer;
  VkDeviceAddress vertex_buffer_addr;
  u32 index_count;
};

struct VulkanContext_impl {
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
  VulkanContext_impl() = default; // Manually initialized
  ~VulkanContext_impl() = default;
  KA_NO_MOVE(VulkanContext_impl);
  KA_NO_COPY(VulkanContext_impl);

public:
  VkInstance vk;
  VkDebugUtilsMessengerEXT messenger;
  VmaAllocator vmalloc;
  VkSurfaceKHR surface;
  VulkanDelQueue delqueue;

  NullTrivial<VulkanDevice> device;
  NullTrivial<VulkanSwapchain> swapchain;
  NullTrivial<VulkanFrameData> framedata;
  NullTrivial<ImDrawData> imdrawdata;
  NullTrivial<DrawThing> draw;
};

fn vk_get_graphics_queue(VulkanContext_impl& ctx) -> VkQueue;

} // namespace kappa::render
