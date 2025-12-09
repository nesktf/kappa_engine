#pragma once

#include "../core.hpp"

namespace kappa::render {

struct handle_t {
  ~handle_t();
};

[[nodiscard]] handle_t initialize();

expect<shogle::texture2d> create_texture(u32 width, u32 height, const void* data,
                                         shogle::image_format format,
                                         shogle::texture_sampler sampler, u32 mipmaps);
expect<std::pair<shogle::texture2d, shogle::framebuffer>> create_framebuffer(u32 width,
                                                                             u32 height);
expect<shogle::shader_storage_buffer> create_ssbo(size_t size, const void* data);
expect<shogle::uniform_buffer> create_ubo(size_t size, const void* data);
expect<shogle::vertex_buffer> create_vbo(size_t size, const void* data);
expect<shogle::index_buffer> create_ebo(size_t size, const void* data);

shogle::framebuffer_view default_fb();

shogle::window& window();
shogle::context_view shogle_context();

template<typename F>
void render_loop(u32 ups, F&& loop) {
  shogle::render_loop(window(), shogle_context(), ups, std::forward<F>(loop));
}

enum tex_sampler_idx {
  TEX_SAMPLER_ALBEDO = 0,
  TEX_SAMPLER_SPECULAR,
  TEX_SAMPLER_NORMALS,
  TEX_SAMPLER_DISPLACEMENT,

  TEX_SAMPLER_COUNT,
};

struct mesh_render_data {
  cspan<shogle::vertex_binding> vertex_buffers;
  shogle::index_buffer_view index_buffer;
  u32 vertex_count;
  u32 vertex_offset;
  u32 index_offset;
  u32 sort_offset;

  vec_span textures;
  vec_span uniforms;
  vec_span bindings;
  shogle::pipeline_view pipeline;
};

struct object_render_data {
  std::vector<mesh_render_data> meshes;
  std::vector<shogle::shader_binding> bindings;
  std::vector<shogle::texture_binding> textures;
  std::vector<shogle::uniform_const> uniforms;
};

struct scene_render_data {
  shogle::uniform_buffer_view transform;
};

struct renderable {
  virtual ~renderable() = default;
  virtual u32 retrieve_render_data(const scene_render_data& scene, object_render_data& data) = 0;
};

void render_thing(shogle::framebuffer_view target, u32 sort, const scene_render_data& scene,
                  renderable& obj);

} // namespace kappa::render
