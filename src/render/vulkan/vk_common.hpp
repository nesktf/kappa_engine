#pragma once

#include "../../util/expected.hpp"

#include <vulkan/vulkan_core.h>

namespace kappa::render {

struct VkContext_Impl;
struct VkAllocBuff_Impl;
struct VkAllocImage_Impl;

typedef struct VkAllocator_Impl* VkMemAllocator;
typedef struct VkHandle_Impl* VkHandle;

constexpr usize MAX_FRAMES_IN_FLIGHT = 2;

fn vk_error_string(VkResult res) noexcept -> const char*;

class VkError : public std::exception {
public:
  VkError() noexcept : _err(VK_SUCCESS) {}

  VkError(VkResult err) noexcept : _err(err) {}

public:
  fn what() const noexcept -> const char* override { return vk_error_string(_err); }

  fn code() const noexcept -> VkResult { return _err; }

private:
  VkResult _err;
};

class VkMsgError : public std::exception {
public:
  VkMsgError(const char* msg, VkResult err) noexcept : _msg(msg), _err(err) {}

public:
  fn what() const noexcept -> const char* override { return _msg; }

  fn code() const noexcept -> VkResult { return _err; }

private:
  const char* _msg;
  VkResult _err;
};

template<typename T>
using VkExpect = Expected<T, VkError>;

template<typename T>
using VkMsgExpect = Expected<T, VkMsgError>;

} // namespace kappa::render
