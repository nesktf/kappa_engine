#pragma once

#include "../assets/texture.hpp"
#include "../core.hpp"

#include <shogle/render/opengl.hpp>
#include <shogle/render/window.hpp>

namespace kappa::render {

void initialize(u32 win_w, u32 win_h);
void destroy();

void start_frame();
void end_frame();

shogle::glfw_win& window();

enum class pipeline_handle : u32 {};
enum class buffer_handle : u32 {};
enum class texture_handle : u32 {};

template<typename T>
constexpr inline bool is_nil_handle(T handle) noexcept {
  return static_cast<u32>(handle) == 0;
}

template<typename T>
constexpr inline T nil_handle() noexcept {
  return static_cast<T>(0);
}

struct texture_create_data {
  const void* data;
  extent2d extent;
  assets::image_format format;
  assets::texture_type type;
};

s_expect<texture_handle> create_texture(const texture_create_data& data);
void destroy_texture(texture_handle tex);

s_expect<buffer_handle> create_buffer(size_t buffer_size);
void update_buffer(buffer_handle buffer, const void* data, size_t data_size, size_t data_offset);
void destroy_buffer(buffer_handle buffer);

} // namespace kappa::render
