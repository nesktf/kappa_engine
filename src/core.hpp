#pragma once

#define NTF_KEYWORD_FN
#include <ntf/core.hpp>

#include <ntf/buffer.hpp>
#include <ntf/expected.hpp>
#include <ntf/freelist.hpp>
#include <ntf/func.hpp>
#include <ntf/func_util.hpp>
#include <ntf/optional.hpp>
#include <ntf/ref.hpp>
#include <ntf/span.hpp>
#include <ntf/unique.hpp>

#define ka_assert(...) NTF_ASSERT(__VA_ARGS__)
#define ka_todo(...)   NTF_TODO(__VA_ARGS__)
#define ka_panic(...)  NTF_PANIC(__VA_ARGS__)

#define KA_UNUSED(_thing) NTF_UNUSED(_thing)
#define KA_UNREACHABLE()  NTF_UNREACHABLE()

#ifndef KA_INTERNAL_
#define KA_DECL_OPAQUE(_typename, _size, _align) \
  typedef struct _typename {                     \
    alignas(_align) uint8_t _data[_size];        \
  } _typename
#else
#define KA_DECL_OPAQUE(_typename, _size, _align)       \
  struct _typename;                                    \
  constexpr size_t _opaque_align_##_typename = _align; \
  constexpr size_t _opaque_size_##_typename = _size;
#endif

#define KA_CHECK_OPAQUE(_typename)                                                     \
  static_assert(alignof(_typename) <= _opaque_align_##_typename, "Invalid alignment"); \
  static_assert(sizeof(_typename) == _opaque_size_##_typename, "Invalid size")

#define KA_SELF_FORWARD(_typename)                                                             \
private:                                                                                       \
  struct create_t {};                                                                          \
  Self self;                                                                                   \
                                                                                               \
public:                                                                                        \
  template<typename... SelfArgs>                                                               \
  _typename(create_t,                                                                          \
            SelfArgs&&... args) noexcept(std::is_nothrow_constructible_v<Self, SelfArgs...>) : \
      self(std::forward<SelfArgs>(args)...) {}

#define KA_VER_MAJ 0
#define KA_VER_MIN 0
#define KA_VER_REV 1

#include <fmt/format.h>

#include <deque>
#include <vector>

namespace kappa {

using namespace ntf::numdefs;

extern int g_argc;
extern char** g_argv;

template<typename T>
using Vec = std::vector<T>;

template<typename T>
using Deque = std::deque<T>;

using ntf::DeferFn;
using ntf::Expected;
using ntf::FixedFreelist;
using ntf::FnRef;
using ntf::FreelistSlot;
using ntf::Nullable;
using ntf::Optional;
using ntf::Ref;
using ntf::Span;
using ntf::TrivFn;
using ntf::TypeArrayBuffer;
using ntf::TypeBuffer;
using ntf::UniqueArray;

using ntf::in_place;
using ntf::make_unique_array;
using ntf::nullopt;
using ntf::unexpect;
using ntf::uninitialized;

template<typename T>
struct TypeBufferRef {
public:
  template<size_t Size, size_t Align>
  TypeBufferRef(TypeBuffer<T, Size, Align>& buffer) : _ref(&buffer) {}

public:
  template<size_t Size = sizeof(T), size_t Align = alignof(T)>
  TypeBuffer<T, Size, Align>& get() const {
    return *static_cast<TypeBuffer<T, Size, Align>*>(_ref);
  }

  template<size_t Size = sizeof(T), size_t Align = alignof(T)>
  TypeBuffer<T, Size, Align>* operator->() const {
    return &get<Size, Align>();
  }

  template<size_t Size = sizeof(T), size_t Align = alignof(T)>
  TypeBuffer<T, Size, Align>& operator*() const {
    return get<Size, Align>();
  }

private:
  void* _ref;
};

enum class LogLevel : u32 {
  error = 0,
  warn,
  info,
  debug,
  verbose,
};

void set_log_level(LogLevel level);
LogLevel get_log_level();
void log_str(std::string_view prefix, std::string_view str);

fn load_entire_file(const char* path) -> UniqueArray<u8>;

template<typename... Args>
void log_at_level(LogLevel level, fmt::format_string<Args...> fmt, Args&&... args) {
  if ((u32)get_log_level() < (u32)level) {
    return;
  }

  static constexpr std::string_view prefixes[] = {
    "[0;31m[ERROR]", "[0;33m[WARNING]", "[0;34m[INFO]", "[0;32m[DEBUG]", "[0;37m[VERBOSE]",
  };

  const auto str = fmt::format(fmt, std::forward<Args>(args)...);
  log_str(prefixes[(u32)level], str);
}

template<typename... Args>
void log_error(fmt::format_string<Args...> fmt, Args&&... args) {
  log_at_level(LogLevel::error, fmt, std::forward<Args>(args)...);
}

template<typename... Args>
void log_warn(fmt::format_string<Args...> fmt, Args&&... args) {
  log_at_level(LogLevel::warn, fmt, std::forward<Args>(args)...);
}

template<typename... Args>
void log_info(fmt::format_string<Args...> fmt, Args&&... args) {
  log_at_level(LogLevel::info, fmt, std::forward<Args>(args)...);
}

template<typename... Args>
void log_debug(fmt::format_string<Args...> fmt, Args&&... args) {
  log_at_level(LogLevel::debug, fmt, std::forward<Args>(args)...);
}

template<typename... Args>
void log_verbose(fmt::format_string<Args...> fmt, Args&&... args) {
  log_at_level(LogLevel::verbose, fmt, std::forward<Args>(args)...);
}

template<size_t MaxSize>
struct BuffStr {
  static constexpr size_t buffer_size = MaxSize;

  char data[MaxSize];
  size_t len;

  const char* c_str() const noexcept { return data; }

  std::string_view as_view() const noexcept { return {&data[0], len}; }

  size_t copy_from(const char* src, size_t size) {
    std::memset(data, 0, MaxSize);
    len = std::min(MaxSize - 1, size);
    std::memcpy(data, src, len);
    data[MaxSize - 1] = '\0'; // Make sure we have a null terminator
    return len;
  }

  size_t copy_from(const char* src) { return copy_from(src, std::strlen(src)); }

  template<typename... Args>
  size_t format_from(fmt::format_string<Args...> fmt, Args&&... args) {
    std::memset(data, 0, MaxSize);
    const auto res = fmt::format_to_n(data, MaxSize - 1, fmt, std::forward<Args>(args)...);
    len = res.size;
    return res.size;
  }
};

template<size_t MaxSize>
BuffStr<MaxSize> buffer_str_copy(const char* data, size_t len) {
  BuffStr<MaxSize> out;
  out.copy_from(data, len);
  return out;
}

template<size_t MaxSize>
BuffStr<MaxSize> buffer_str_copy(const char* data) {
  BuffStr<MaxSize> out;
  out.copy_from(data);
  return out;
}

template<size_t MaxSize, typename... Args>
BuffStr<MaxSize> buffer_str_fmt(fmt::format_string<Args...> fmt, Args&&... args) {
  BuffStr<MaxSize> out;
  out.format_from(fmt, std::forward<Args>(args)...);
  return out;
}

} // namespace kappa
