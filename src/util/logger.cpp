#include "./logger.hpp"

#include <fmt/chrono.h>
#include <fmt/printf.h>

#ifdef NDEBUG
#define KA_ABORT() ::std::abort()
#else
#ifdef __clang__
#define KA_ABORT() __builtin_debugtrap()
#else
#define KA_ABORT() __builtin_trap()
#endif
#endif

#define PANIC_PREFIX "\033[0;31m[PANIC]\033[0m {}:{}@{}:"

namespace kappa {

namespace {
LogLevel level = LogLevel::verbose;

fn get_time() {

  using TimePoint = std::chrono::system_clock::time_point;
  TimePoint now = std::chrono::system_clock::now();
  uint64_t ms =
    std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;

  return std::make_pair(std::chrono::system_clock::to_time_t(now), ms);
}
} // namespace

void set_log_level(LogLevel level_) {
  level = level_;
}

void log_str(std::string_view prefix, std::string_view str) {
  const auto [time, ms] = get_time();
  const std::tm* time_tm = std::localtime(&time);
  fmt::print("[{:%H:%M:%S}.{:03d}]\033{}\033[0m{}\n", *time_tm, (int)ms, prefix, str);
}

namespace impl {

[[noreturn]] void assert_failure(const char* cond, const char* file, const char* func, int line,
                                 const char* msg) {
  if (msg) {
    fmt::print(PANIC_PREFIX " assertion '{}' failed: {}", file, line, func, cond, msg);
  } else {
    fmt::print(PANIC_PREFIX " assertion '{}' failed", file, line, func, cond);
  }
  KA_ABORT();
  KA_UNREACHABLE();
}

[[noreturn]] void on_panic(const char* file, const char* func, int line, const char* msg) {
  fmt::print(PANIC_PREFIX " {}", file, line, func, msg);
  KA_ABORT();
  KA_UNREACHABLE();
}

} // namespace impl

} // namespace kappa
