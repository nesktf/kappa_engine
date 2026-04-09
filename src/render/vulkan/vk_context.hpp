#pragma once

#include "../../util/expected.hpp"
#include "../../util/function.hpp"
#include "../../util/ptr.hpp"

#include <vulkan/vulkan_core.h>

namespace kappa::render {

fn vk_error_string(VkResult result) noexcept -> const char*;

class VkError : public std::exception {
public:
  VkError(VkResult err) : _err(err) {}

public:
  fn what() const noexcept -> const char* override { return vk_error_string(_err); }

  fn code() const noexcept -> VkResult { return _err; }

private:
  VkResult _err;
};

class VkStrError : public std::exception {
public:
  VkStrError(std::string msg, VkResult err) : _msg(std::move(msg)), _err(err) {}

public:
  fn what() const noexcept -> const char* override { return _msg.c_str(); }

  fn code() const noexcept -> VkResult { return _err; }

  fn code_msg() const noexcept -> std::string_view { return vk_error_string(_err); }

private:
  std::string _msg;
  VkResult _err;
};

class VkSvError : public std::exception {
public:
  VkSvError(const char* msg, VkResult err) noexcept : _msg(msg), _err(err) {}

public:
  fn what() const noexcept -> const char* override { return _msg; }

  fn code() const noexcept -> VkResult { return _err; }

  fn code_msg() const noexcept -> std::string_view { return vk_error_string(_err); }

private:
  const char* _msg;
  VkResult _err;
};

template<typename T>
using VkSExpect = Expected<T, VkSvError>;

template<typename T>
using VkSvExpect = Expected<T, VkSvError>;

struct VulkanInfo {
  const char* app_name;
  u32 app_ver;
};

struct VulkanContextImpl;

class VulkanContext {
public:
  using SurfaceProviderFn =
    TrivFn<VkResult(VkInstance, VkSurfaceKHR*, const VkAllocationCallbacks*), 2 * sizeof(void*),
           8>;

  struct Deleter {
    void operator()(VulkanContextImpl* impl) noexcept;
  };

public:
  VulkanContext(VulkanContextImpl& impl) noexcept : _impl(&impl) {}

public:
  static fn create(const VulkanInfo& app_info, VkExtent2D surface_extent,
                   Span<const char*> surface_extensions, SurfaceProviderFn surface_provider)
    -> VkSvExpect<VulkanContext>;

public:
  fn rebuild_swapchain(VkExtent2D surface_extent) -> VkSvExpect<void>;

  fn draw() -> void;

private:
  std::unique_ptr<VulkanContextImpl, Deleter> _impl;
};

} // namespace kappa::render
