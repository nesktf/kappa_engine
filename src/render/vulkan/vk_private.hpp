#pragma once

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

#include "./vk.hpp"

#include "../../util/logger.hpp"

#include <vk_mem_alloc.h>

#define KA_VULKAN_VERSION VK_API_VERSION_1_3
#define KA_ENGINE_VER     VK_MAKE_VERSION(KA_VER_MAJ, KA_VER_MIN, KA_VER_REV)
#define KA_ENGINE_NAME    "kappa"

namespace kappa::render {

constexpr auto validation_layers = std::to_array<const char*>({
  "VK_LAYER_KHRONOS_validation",
});

constexpr VkAllocationCallbacks* vkalloc = nullptr;

constexpr usize MAX_FRAMES_IN_FLIGHT = 2;

} // namespace kappa::render
