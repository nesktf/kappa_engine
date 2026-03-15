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
  image_format format;
};

s_expect<texture_handle> create_texture(const texture_create_data& data);
void destroy_texture(texture_handle tex);

s_expect<buffer_handle> create_buffer(size_t buffer_size);
void update_buffer(buffer_handle buffer, const void* data, size_t data_size, size_t data_offset);
void destroy_buffer(buffer_handle buffer);

enum vertex_attrib_flags : bits32 {
  ATTRIB_FLAG_NONE = 0x0000,
  ATTRIB_FLAG_POSITIONS = 0x0001,
  ATTRIB_FLAG_NORMALS = 0x0002,
  ATTRIB_FLAG_TANGENTS = 0x0004,
  ATTRIB_FLAG_BONES = 0x0008,
  ATTRIB_FLAG_UV0 = 0x0010,
  ATTRIB_FLAG_COLOR0 = 0x0100,
  ATTRIB_FLAG_ALL = 0xFFFF,
};

enum material_tex_flags : bits32 {
  MATERIAL_TEX_FLAG_NONE = 0x0000,
  MATERIAL_TEX_FLAG_DIFFUSE = 0x0001,
  MATERIAL_TEX_FLAG_ALBEDO = 0x0001,
  MATERIAL_TEX_FLAG_SPECULAR = 0x0002,
  MATERIAL_TEX_FLAG_NORMAL = 0x0004,
  MATERIAL_TEX_FLAG_AMBIENT_OCCLUSION = 0x0008,
  MATERIAL_TEX_FLAG_ROUGHNESS = 0x0010,
  MATERIAL_TEX_FLAG_METALLIC = 0x0020,
  MATERIAL_TEX_FLAG_ALL = 0xFFFF,
};

constexpr inline fn flag_from_tex_type(texture_type type) -> bits32 {
  switch (type) {
    case texture_type::albedo:
      return MATERIAL_TEX_FLAG_ALBEDO;
    case texture_type::normal:
      return MATERIAL_TEX_FLAG_NORMAL;
    case texture_type::specular:
      return MATERIAL_TEX_FLAG_SPECULAR;
    case texture_type::ambient_occlusion:
      return MATERIAL_TEX_FLAG_AMBIENT_OCCLUSION;
    case texture_type::roughness:
      return MATERIAL_TEX_FLAG_ROUGHNESS;
    case texture_type::metallic:
      return MATERIAL_TEX_FLAG_METALLIC;
    default:
      return 0;
  }
}

struct pipeline_create_data {
  bits32 vertex_attributes;
  bits32 material_textures;
};

s_expect<pipeline_handle> create_pipeline(const pipeline_create_data& data);
void destroy_pipeline(pipeline_handle pipeline);

} // namespace kappa::render
