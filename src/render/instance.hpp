#pragma once

#include "../core.hpp"

#include <chrono>
#include <string_view>

namespace kappa::render {

using ResHandle = u64;

fn initialize(u32 win_w, u32 win_h) -> void;
fn destroy() -> void;

fn start_frame() -> void;
fn end_frame() -> void;
fn device_wait() -> void;
fn flag_dirty_framebufer() -> void;

fn win_poll_events() -> void;
fn win_should_close() -> bool;

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

namespace meta {

template<typename F>
concept delta_render_func = std::invocable<F, f64>; // f(dt) -> void

template<typename T>
concept delta_render_object = requires(T obj, f64 delta_time) {
  { obj.on_render(delta_time) } -> std::same_as<void>;
};

template<typename F>
concept fixed_render_func = std::invocable<F, f64, f64>; // f(dt, alpha) -> void

template<typename F, u32 UPS>
concept fixed_update_func =
  std::invocable<F, u32> || std::invocable<F, std::integral_constant<u32, UPS>>; // f(ups) -> void

template<typename T>
concept fixed_render_object = requires(T obj, f64 delta_time, f64 alpha) {
  { obj.on_render(delta_time, alpha) } -> std::convertible_to<void>;
};

template<typename T, u32 UPS>
concept fixed_update_object = requires(T obj, u32 fixed_delta) {
  { obj.on_fixed_update(fixed_delta) } -> std::same_as<void>;
} || requires(T obj) {
  { obj.on_fixed_update(std::integral_constant<u32, UPS>{}) } -> std::same_as<void>;
};

template<typename T, u32 UPS>
concept fixed_loop_object = (fixed_render_func<T> && fixed_update_func<T, UPS>) ||
                            (fixed_render_object<T> && fixed_update_object<T, UPS>);

template<typename T>
concept delta_loop_object = delta_render_func<T> || delta_render_object<T>;

} // namespace meta

template<meta::delta_loop_object LoopObj>
void render_loop(LoopObj&& obj) {
  using namespace std::literals;

  using clock = std::chrono::steady_clock;
  using duration = clock::duration;
  using time_point = std::chrono::time_point<clock, duration>;

  time_point last_time = clock::now();
  while (!win_should_close()) {
    const time_point start_time = clock::now();
    const auto elapsed_time = start_time - last_time;
    last_time = start_time;
    const f64 dt = (std::chrono::duration<f64>(elapsed_time) / 1s);

    win_poll_events();
    if constexpr (meta::delta_render_object<LoopObj>) {
      obj.on_render(dt);
    } else {
      obj(dt);
    }
  }
};

template<u32 UPS, meta::fixed_loop_object<UPS> LoopObj>
void render_loop(LoopObj&& obj) {
  using namespace std::literals;

  static constexpr std::chrono::duration<f64> fixed_elapsed_time =
    std::chrono::microseconds(1000000 / UPS);

  using clock = std::chrono::steady_clock;
  using duration = decltype(clock::duration{} + fixed_elapsed_time);
  using time_point = std::chrono::time_point<clock, duration>;

  time_point last_time = clock::now();
  duration lag = 0s;
  while (!win_should_close()) {
    const time_point start_time = clock::now();
    const auto elapsed_time = start_time - last_time;
    last_time = start_time;
    lag += elapsed_time;

    const f64 dt = (std::chrono::duration<f64>(elapsed_time) / 1s);
    const f64 alpha = (std::chrono::duration<f64>(lag) / fixed_elapsed_time);

    win_poll_events();

    while (lag >= fixed_elapsed_time) {
      if constexpr (meta::fixed_update_object<LoopObj, UPS>) {
        obj.on_fixed_update(std::integral_constant<u32, UPS>{});
      } else {
        obj(std::integral_constant<u32, UPS>{});
      }
      lag -= fixed_elapsed_time;
    }

    if constexpr (meta::fixed_render_object<LoopObj>) {
      obj.on_render(dt, alpha);
    } else {
      obj(dt, alpha);
    }
  }
}

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

constexpr size_t MAX_PIPELINES = 512;
constexpr size_t MAX_TEXTURES = 512;
constexpr size_t MAX_BUFFERS = 512;

constexpr u32 glsl_scene_binding = 1;
constexpr u32 glsl_instance_binding = 2;
constexpr u32 glsl_bone_mat_binding = 3;
constexpr u32 glsl_u_samplers = 1;

struct pipeline_data {
  shogle::gl_pipeline pipeline;
  shogle::gl_vertex_layout layout;
};

struct render_context {
public:
  render_context(shogle::gl_context&& gl_, shogle::glfw_win&& win_,
                 shogle::gl_texture&& default_tex_);

public:
  shogle::glfw_win win;
  shogle::gl_context gl;
  shogle::glfw_imgui imgui;
  shogle::gl_texture default_texture;
  inplace_freelist<pipeline_data, MAX_PIPELINES> pipelines;
  inplace_freelist<shogle::gl_texture, MAX_TEXTURES> textures;
  inplace_freelist<shogle::gl_buffer, MAX_BUFFERS> buffers;
  m4f32 proj{1.f};
};

extern shogle::nullable<render_context> g_ctx;

#endif
} // namespace kappa::render
