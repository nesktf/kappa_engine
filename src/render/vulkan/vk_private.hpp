#pragma once

#include "../../util/array.hpp"
#include "../../util/logger.hpp"

#define VK_LOG(_level, _msg, ...) \
  ::kappa::log_##_level("[VULKAN] " _msg __VA_OPT__(, ) __VA_ARGS__)

#define VK_ASSERT(func)                                                             \
  {                                                                                 \
    VkResult vkres = (func);                                                        \
    if (vkres != VK_SUCCESS) {                                                      \
      VK_LOG(error, "ASSERT FAILURE: {}", ::kappa::render::vk_error_string(vkres)); \
      ka_panic("VULKAN ERROR");                                                     \
    }                                                                               \
  }

#include <vulkan/vulkan_core.h>

#include <vk_mem_alloc.h>

#define KA_VULKAN_VERSION VK_API_VERSION_1_3
#define KA_ENGINE_VER     VK_MAKE_VERSION(KA_VER_MAJ, KA_VER_MIN, KA_VER_REV)
#define KA_ENGINE_NAME    "kappa"

namespace kappa::render {

struct VulkanSwapchain {
  VkSwapchainKHR swapchain;
  VkFormat format;
  UniqueArray<VkImage> images;
  UniqueArray<VkImageView> image_views;
  VkExtent2D extent;
};

struct VulkanDevice {
  VkDevice device;
  VkPhysicalDevice physical_device;
  u32 graphics_queue;
  u32 present_queue;
  u32 transfer_queue;
  VkSurfaceCapabilitiesKHR surface_capabilities;
  Vec<VkSurfaceFormatKHR> surface_formats;
  Vec<VkPresentModeKHR> surface_present_modes;
};

struct VulkanFrameData {
  VkCommandPool cmdpool;
  VkCommandBuffer cmdbuf;
};

constexpr usize MAX_FRAMES_IN_FLIGHT = 2;

struct VulkanContextImpl {
  VkInstance vk;
  VkDebugUtilsMessengerEXT messenger;
  VmaAllocator vmalloc;
  VulkanDevice device;
  VkSurfaceKHR surface;
  VulkanSwapchain swapchain;

  VulkanFrameData frames[MAX_FRAMES_IN_FLIGHT];
};

#if 0

  const fn create_cmdpool = [&](const GraphicsDevice& device) -> RenderContext::CommandPool {
    // Commands in vulkan are recorded in a command buffer
    // instead of being executed directly using function calls
    VkCommandPoolCreateInfo pool{};
    pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

    const auto queue_families = device.queue_families();
    // The pool will be used for drawing commands only
    pool.queueFamilyIndex = queue_families.graphics;

    // All command buffers can be rerecorded individually
    pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkCommandPool graphics_pool;
    VK_ASSERT(vkCreateCommandPool(device, &pool, nullptr, &graphics_pool));

    VkCommandPool transfer_pool;
    pool.queueFamilyIndex = queue_families.transfer;
    VK_ASSERT(vkCreateCommandPool(device, &pool, nullptr, &transfer_pool));

    // Command buffer allocation
    std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> graphics_cmdbufs;

    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = graphics_pool;
    alloc.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    // If the command buffer can be submited directly but not called from other buffers
    // or cannot be submitted directly but can be called from pirmary buffers
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    VK_ASSERT(vkAllocateCommandBuffers(device, &alloc, graphics_cmdbufs.data()));

    VkCommandBuffer transfer_command_buffer;
    alloc.commandPool = transfer_pool;
    alloc.commandBufferCount = 1;
    VK_ASSERT(vkAllocateCommandBuffers(device, &alloc, &transfer_command_buffer));
    return {graphics_pool, transfer_pool, graphics_cmdbufs, transfer_command_buffer};
  };

  const fn create_sync = [&](const GraphicsDevice& device) -> RenderContext::SyncObjects {
    // Steps for drawing a frame:
    // 1. Wait for the previous frame to finish
    // 2. Acquire an image from the swapchain
    // 3. Record a command buffer which draws an scene onto that image
    // 4. Submit the recorded command buffer (and execute it)
    // 5. Present the swapchain image

    // Steps 2, 4 and 5 hapen asynchronously on the GPU, so some
    // synchronization primitives are needed: semaphores and fences
    // - Semaphores are for synchronizing the GPU (Blocks between queue commands)
    // - Fences are for synchronizing the CPU (Blocks until some queue command finishes)

    // Semaphores should be used for swapchain operations (because they happen on the GPU)
    // Fences are used when we're waiting for the previous frame to end before drawing again, so
    // we don't overwrite the current contents of the command buffer while the GPU is using it

    VkSemaphoreCreateInfo semaphore{};
    semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence{};
    fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    // Create the fence signaled, since there is no previous frame on the first frame
    fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    RenderContext::SyncObjects objs;
    u32 i = 0;
    for (; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      auto& image_sem = objs.image_avail_sem[i];
      auto& render_sem = objs.render_end_sem[i];
      auto& in_flight_fen = objs.in_flight_fences[i];
      VK_ASSERT(vkCreateSemaphore(device, &semaphore, nullptr, &image_sem));
      VK_ASSERT(vkCreateSemaphore(device, &semaphore, nullptr, &render_sem))
      VK_ASSERT(vkCreateFence(device, &fence, nullptr, &in_flight_fen));
    }
    return objs;
  };

#endif

} // namespace kappa::render
