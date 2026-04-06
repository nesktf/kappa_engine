#pragma once

#include "../defs.hpp"

namespace kappa {

template<usize MaxSize>
struct BufferStr {
  static constexpr usize buffer_size = MaxSize;

  char data[MaxSize];
  usize len;

  const char* c_str() const noexcept { return data; }

  std::string_view as_view() const noexcept { return {&data[0], len}; }

  usize copy_from(const char* src, usize size) {
    std::memset(data, 0, MaxSize);
    len = std::min(MaxSize - 1, size);
    std::memcpy(data, src, len);
    data[MaxSize - 1] = '\0'; // Make sure we have a null terminator
    return len;
  }

  usize copy_from(const char* src) { return copy_from(src, std::strlen(src)); }

  template<typename... Args>
  usize format_from(fmt::format_string<Args...> fmt, Args&&... args) {
    std::memset(data, 0, MaxSize);
    const auto res = fmt::format_to_n(data, MaxSize - 1, fmt, std::forward<Args>(args)...);
    len = res.size;
    return res.size;
  }
};

template<usize MaxSize>
BufferStr<MaxSize> buffer_str_copy(const char* data, usize len) {
  BufferStr<MaxSize> out;
  out.copy_from(data, len);
  return out;
}

template<usize MaxSize>
BufferStr<MaxSize> buffer_str_copy(const char* data) {
  BufferStr<MaxSize> out;
  out.copy_from(data);
  return out;
}

template<usize MaxSize, typename... Args>
BufferStr<MaxSize> buffer_str_fmt(fmt::format_string<Args...> fmt, Args&&... args) {
  BufferStr<MaxSize> out;
  out.format_from(fmt, std::forward<Args>(args)...);
  return out;
}

} // namespace kappa
