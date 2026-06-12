#pragma once

#define KA_INTERNAL_

#define KA_VK_LOG(_level, _msg, ...) \
  ::kappa::log_##_level("[VULKAN] " _msg __VA_OPT__(, ) __VA_ARGS__)

#define KA_VK_ASSERT(func)                                                             \
  {                                                                                    \
    VkResult vkres = (func);                                                           \
    if (vkres != VK_SUCCESS) {                                                         \
      KA_VK_LOG(error, "ASSERT FAILURE: {}", ::kappa::render::vk_error_string(vkres)); \
      ka_panic("VULKAN ERROR");                                                        \
    }                                                                                  \
  }

#define KA_VK_UNEX(_func)                                    \
  if (auto _vkret = (_func); _vkret != VK_SUCCESS) {         \
    KA_VK_LOG(error, "evil path @ {}", __PRETTY_FUNCTION__); \
    return {unexpect, _vkret};                               \
  }

#define KA_VK_UNEX_STR(_func, _str)                          \
  if (auto _vkret = (_func); _vkret != VK_SUCCESS) {         \
    KA_VK_LOG(error, "evil path @ {}", __PRETTY_FUNCTION__); \
    return {unexpect, _str, _vkret};                         \
  }

#define KA_VK_UNEX_FMT(_func, _fmt, ...)                                       \
  if (auto _vkret = (_func); _vkret != VK_SUCCESS) {                           \
    KA_VK_LOG(error, "evil path @ {}", __PRETTY_FUNCTION__);                   \
    return {unexpect, ::fmt::format(_fmt __VA_OPT__(, ) __VA_ARGS__), _vkret}; \
  }

#include "./vk_buffer.hpp"
#include "./vk_context.hpp"
#include "./vk_device.hpp"
#include "./vk_swapchain.hpp"
#include "./vk_util.hpp"

#include "../../util/logger.hpp"

#include <vk_mem_alloc.h>

#include <array>

#define KA_VULKAN_VERSION VK_API_VERSION_1_3
#define KA_ENGINE_VER     VK_MAKE_VERSION(KA_VER_MAJ, KA_VER_MIN, KA_VER_REV)
#define KA_ENGINE_NAME    "kappa"

constexpr VkAllocationCallbacks* vkalloc = nullptr;

namespace kappa::render {

constexpr auto validation_layers = std::to_array<const char*>({
  "VK_LAYER_KHRONOS_validation",
});

struct VkContext_Impl {
public:
  struct ImDrawData {
    VkFence fence;
    VkCommandPool cmdpool;
    VkCommandBuffer cmdbuf;
  };

public:
  VkContext_Impl(VkInstance vk_, VkDebugUtilsMessengerEXT messenger_, VmaAllocator vmalloc_,
                 VkSurfaceKHR surface_, VkContextDevice&& device_, VkSwapchain&& swapchain_,
                 VkFrameData&& framedata_, ImDrawData&& imdrawdata_, VkDelQueue&& delqueue_,
                 Optional<VkUpdateSurfExtFn>&& update_surf_);
  ~VkContext_Impl();
  KA_NO_MOVE(VkContext_Impl);
  KA_NO_COPY(VkContext_Impl);

public:
  VkInstance vk;
  VkDebugUtilsMessengerEXT messenger;
  VmaAllocator vmalloc;
  VkSurfaceKHR surface;
  ImDrawData imdrawdata;
  VkContextDevice device;
  VkSwapchain swapchain;
  VkFrameData framedata;
  VkDelQueue delqueue;
  Optional<VkUpdateSurfExtFn> update_surf;
};

} // namespace kappa::render
