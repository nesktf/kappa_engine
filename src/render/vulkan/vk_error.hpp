#pragma once

#include "./vk.h"

#include "../../util/expected.hpp"

namespace kappa::render {

class VkError : public std::exception {
public:
  VkError() noexcept : _err(VK_SUCCESS) {}

  VkError(VkResult err) noexcept : _err(err) {}

public:
  fn what() const noexcept -> const char* override { return ka_vk_error_string(_err); }

  fn code() const noexcept -> VkResult { return _err; }

private:
  VkResult _err;
};

template<typename T>
using VkExpect = Expected<T, VkError>;

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
