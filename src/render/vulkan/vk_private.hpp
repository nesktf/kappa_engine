#pragma once

#include "../../util/array.hpp"
#include "../../util/function.hpp"
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
  Vec<VkSurfaceFormatKHR> surface_formats;
  Vec<VkPresentModeKHR> surface_present_modes;
};

struct VulkanImage {
  VkImage image;
  VkImageView view;
  VmaAllocation alloc;
  VkExtent3D extent;
  VkFormat format;
};

struct DeletionQueue {
  using DeleteFn = TrivFn<void(), 4 * sizeof(void*), 8>;
  Vec<DeleteFn> deletors;

  fn push_fn(DeleteFn&& func) -> void { deletors.push_back(std::move(func)); }

  fn flush() -> void {
    for (auto it = deletors.rbegin(); it != deletors.rend(); ++it) {
      (*it)();
    }
    deletors.clear();
  }
};

struct VulkanFrameData {
  VkCommandPool cmdpool;
  VkCommandBuffer cmdbuf;
  VkSemaphore swapchain_sem, render_sem;
  VkFence render_fen;
  DeletionQueue delqueue;
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
  u32 curr_frame;

  VulkanImage draw_image;
  VkExtent2D draw_extent;
};

} // namespace kappa::render
