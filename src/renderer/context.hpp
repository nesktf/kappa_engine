#pragma once

#define SHOGLE_EXPOSE_GLFW 1
#include "../common.hpp"

#include <ntfstl/singleton.hpp>
#include <variant>

namespace kappa {

enum vert_shader_type {
  VERT_SHADER_RIGGED_MODEL = 0,
  VERT_SHADER_STATIC_MODEL,
  VERT_SHADER_GENERIC_MODEL,
  VERT_SHADER_SKYBOX,
  VERT_SHADER_SPRITE,
  VERT_SHADER_EFFECT,

  VERT_SHADER_COUNT,
};

enum frag_shader_type {
  FRAG_SHADER_RAW_ALBEDO = 0,

  FRAG_SHADER_COUNT,
};

enum tex_sampler_idx {
  TEX_SAMPLER_ALBEDO = 0,
  TEX_SAMPLER_SPECULAR,
  TEX_SAMPLER_NORMALS,
  TEX_SAMPLER_DISPLACEMENT,

  TEX_SAMPLER_COUNT,
};

class tex_sampler_data {
public:
  tex_sampler_data() noexcept = default; // trivial

  tex_sampler_data(u32 binding) noexcept {
    for (auto& sampler : data) {
      sampler = binding;
    }
  }

  tex_sampler_data& set(u32 idx, u32 binding) {
    NTF_ASSERT(idx < TEX_SAMPLER_COUNT);
    data[idx] = binding;
    return *this;
  }

public:
  std::array<u32, TEX_SAMPLER_COUNT> data;
  static_assert(sizeof(data) % 16 == 0, "Not 16 byte aligned");
};

struct pipeline_opts {
  shogle::render_tests tests;
  shogle::primitive_mode primitive;
  bool use_aos_bindings;
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

class renderer : public ntf::singleton<renderer> {
private:
  friend ntf::singleton<renderer>;

  struct handle_t {
    ~handle_t() { renderer::destroy(); }
  };

  using vert_shader_array = std::array<shogle::vertex_shader, VERT_SHADER_COUNT>;

public:
  static constexpr i32 VERT_MODEL_TRANSFORM_LOC = 1;
  static constexpr i32 VERT_SCENE_TRANSFORM_LOC = 2;

  static constexpr i32 FRAG_SAMPLER_LOC = 8;

private:
  renderer(shogle::window&& win, shogle::context&& ctx, vert_shader_array&& vert_shaders);

public:
  static handle_t construct();

public:
  expect<shogle::pipeline> make_pipeline(vert_shader_type vert, frag_shader_type frag,
                                       std::vector<shogle::attribute_binding>& bindings,
                                       const pipeline_opts& opts);

  void render(shogle::framebuffer_view target, u32 sort,
              const scene_render_data& scene, renderable& obj);

private:
  shogle::vertex_shader_view _make_vert_stage(vert_shader_type type,
                                            u32& flags,
                                            std::vector<shogle::attribute_binding>& bindings,
                                            bool aos_bindings);
  expect<shogle::fragment_shader> _make_frag_stage(frag_shader_type type, u32 vert_flags);

public:
  shogle::window& win() { return _win; }
  shogle::context_view ctx() const { return _ctx; }

public:
  void render_loop(auto&& fun) {
    shogle::render_loop(_win, _ctx, GAME_UPS, std::forward<decltype(fun)>(fun));
  }

private:
  shogle::window _win;
  shogle::context _ctx;
  vert_shader_array _vert_shaders;
  object_render_data _render_data;
};

} // namespace kappa
