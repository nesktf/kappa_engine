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

#include "./vk.hpp"

#include "../../util/logger.hpp"

#include <vk_mem_alloc.h>

#include <array>

#define KA_VULKAN_VERSION VK_API_VERSION_1_3
#define KA_ENGINE_VER     VK_MAKE_VERSION(KA_VER_MAJ, KA_VER_MIN, KA_VER_REV)
#define KA_ENGINE_NAME    "kappa"

constexpr VkAllocationCallbacks* vkalloc = nullptr;

namespace kappa::render {

class VulkanDelQueue;
class VulkanDevice;
class VulkanSwapchain;
class VulkanFrameData;

constexpr auto validation_layers = std::to_array<const char*>({
  "VK_LAYER_KHRONOS_validation",
});

constexpr usize MAX_FRAMES_IN_FLIGHT = 2;

} // namespace kappa::render
