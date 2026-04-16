#pragma once

#include "../../util/expected.hpp"

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
using VkExpect = Expected<T, VkError>;

template<typename T>
using VkSExpect = Expected<T, VkSvError>;

template<typename T>
using VkSvExpect = Expected<T, VkSvError>;

} // namespace kappa::render
