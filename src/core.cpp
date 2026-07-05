#include "./core.hpp"

#include <iostream>

#include <fmt/chrono.h>
#include <fmt/printf.h>

#include <fstream>

namespace kappa {

fn load_entire_file(const char* path) -> UniqueArray<u8> {
  std::ifstream file(path, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    return {};
  }
  const size_t filesz = (size_t)file.tellg();
  auto array = ntf::make_unique_array<u8>(ntf::uninitialized, filesz);
  file.seekg(0);
  file.read((char*)array.data(), filesz);
  file.close();
  return array;
}

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

LogLevel get_log_level() {
  return level;
}

void log_str(std::string_view prefix, std::string_view str) {
  const auto [time, ms] = get_time();
  const std::tm* time_tm = std::localtime(&time);
  fmt::print("[{:%H:%M:%S}.{:03d}]\033{}\033[0m{}\n", *time_tm, (int)ms, prefix, str);
}

} // namespace kappa
