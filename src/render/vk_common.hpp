#pragma once

#include "../util/expected.hpp"

#include "../util/logger.hpp"

#define VK_LOG(_level, _msg, ...) \
  ::kappa::log_##_level("[VULKAN] " _msg __VA_OPT__(, ) __VA_ARGS__)

#include <string>

#define VK_ASSERT(func)         \
  {                             \
    VkResult vkres = (func);    \
    if (vkres != VK_SUCCESS) {  \
      ka_panic("VULKAN ERROR"); \
    }                           \
  }

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

namespace kappa::render {

constexpr auto base_device_extensions = std::to_array<const char*>(
  {VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_DEVICE_GROUP_EXTENSION_NAME});
constexpr auto validation_layers = std::to_array<const char*>({
  "VK_LAYER_KHRONOS_validation",
#ifndef NDEBUG
  VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
});

template<typename T>
using VkView = T;

fn vk_error_string(VkResult result) noexcept -> std::string_view;

class VkError : public std::exception {
public:
  VkError(std::string msg, VkResult err) : _msg(std::move(msg)), _err(err) {}

public:
  const char* what() const noexcept override { return _msg.c_str(); }

  VkResult code() const noexcept { return _err; }

  std::string_view code_msg() const noexcept { return vk_error_string(_err); }

private:
  std::string _msg;
  VkResult _err;
};

class VkSvError : public std::exception {
public:
  VkSvError(const char* msg, VkResult err) noexcept : _msg(msg), _err(err) {}

public:
  const char* what() const noexcept override { return _msg; }

  VkResult code() const noexcept { return _err; }

  std::string_view code_msg() const noexcept { return vk_error_string(_err); }

private:
  const char* _msg;
  VkResult _err;
};

template<typename T>
using VkSExpect = Expected<T, VkSvError>;

template<typename T>
using VkSvExpect = Expected<T, VkSvError>;

template<typename T>
requires(std::is_arithmetic_v<T>)
constexpr fn to_vk_extent2d(Extent2D extent) -> VkExtent2D {
  const auto [width, height] = extent;
  return VkExtent2D{.width = static_cast<u32>(width), .height = static_cast<u32>(height)};
}

} // namespace kappa::render
