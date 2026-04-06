#pragma once

#include "../core.hpp"

#include <fmt/format.h>

namespace kappa {

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

} // namespace kappa
