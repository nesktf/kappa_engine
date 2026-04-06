#pragma once

#include "../core.hpp"

#include <string_view>

namespace kappa::render {

using ResHandle = u64;

fn initialize(u32 win_w, u32 win_h) -> void;
fn destroy() -> void;

fn start_frame() -> void;
fn end_frame() -> void;
fn device_wait() -> void;
fn flag_dirty_framebufer() -> void;

struct IndexedDrawCmd {
  ResHandle pipeline;
  ResHandle vertex_buffer;
  ResHandle index_buffer;
  u32 indices;
};

fn record_command(const IndexedDrawCmd& cmd) -> void;

struct LayoutInfo {};

fn create_pipeline(const LayoutInfo& layout, std::string_view vert, std::string_view frag)
  -> ResHandle;

fn destroy_pipeline(ResHandle pipeline) -> void;

enum class VkBufferType {
  vertex = 0,
  index,
  uniform,
};

fn create_buffer(VkBufferType type, usize size, bits32 flags) -> ResHandle;

fn upload_buffer_data(ResHandle buffer, const void* data, usize size, usize offset) -> void;

fn destroy_buffer(ResHandle buffer) -> void;

#if 0
shogle::glfw_win& window();

enum class pipeline_handle : u32 {};
enum class buffer_handle : u32 {};
enum class texture_handle : u32 {};

template<typename T>
constexpr inline bool is_nil_handle(T handle) noexcept {
  return static_cast<u32>(handle) == 0xFFFFFFFF;
}

template<typename T>
constexpr inline T nil_handle() noexcept {
  return static_cast<T>(0xFFFFFFFF);
}

struct texture_create_data {
  const void* data;
  extent2d extent;
  image_format format;
};

texture_handle create_texture(const texture_create_data& data);
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
  buffer_handle buffer;
  size_t nverts;
  size_t vertex_offset;
  size_t index_offset;
  bits32 vertex_attributes;
  bits32 material_textures;
};

s_expect<pipeline_handle> create_pipeline(const pipeline_create_data& data);
void destroy_pipeline(pipeline_handle pipeline);

static constexpr size_t MAX_BOUND_TEXTURES = 8;
static constexpr size_t MAX_SHADER_BINDS = 8;

struct shader_binding {
  size_t size;
  size_t offset;
  buffer_handle buffer;
  u32 location;
};

struct texture_binding {
  texture_handle texture;
  u32 location;
};

#if 0
struct uniform_data {
  alignas(m4f32) u8 data[sizeof(m4f32)];
  shogle::attribute_type type;
  u32 location;
};
#endif

struct render_data {
  pipeline_handle pipeline;
  buffer_handle mesh_buffer;
  std::array<texture_binding, MAX_BOUND_TEXTURES> textures;
  size_t texture_count;
  std::array<shader_binding, MAX_SHADER_BINDS> shader_binds;
  size_t shader_bind_count;
#if 0
  std::array<uniform_data, MAX_SHADER_BINDS> uniforms;
	size_t uniform_count;
#endif
  size_t draw_count;
  size_t instances;
};

void submit_render_batch(span<const render_data> data);
#endif

} // namespace kappa::render
