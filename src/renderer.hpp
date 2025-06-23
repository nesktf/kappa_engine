#pragma once

#define SHOGLE_EXPOSE_GLFW 1
#include "common.hpp"
#include <shogle/boilerplate.hpp>

#include <ntfstl/singleton.hpp>
#include <variant>

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

struct pipeline_opts {
  ntfr::render_tests tests;
  ntfr::primitive_mode primitive;
  bool use_aos_bindings;
};

class renderer : public ntf::singleton<renderer> {
private:
  friend ntf::singleton<renderer>;

  struct handle_t {
    ~handle_t() { renderer::destroy(); }
  };

  using vert_shader_array = std::array<ntfr::vertex_shader, VERT_SHADER_COUNT>;

private:
  renderer(ntfr::window&& win, ntfr::context&& ctx, vert_shader_array&& vert_shaders);

public:
  static handle_t construct();

public:
  expect<ntfr::pipeline> make_pipeline(vert_shader_type vert, frag_shader_type frag,
                                       std::vector<ntfr::attribute_binding>& bindings,
                                       const pipeline_opts& opts);

private:
  ntfr::vertex_shader_view _make_vert_stage(vert_shader_type type,
                                            u32& flags,
                                            std::vector<ntfr::attribute_binding>& bindings,
                                            bool aos_bindings);
  expect<ntfr::fragment_shader> _make_frag_stage(frag_shader_type type, u32 vert_flags);

public:
  ntfr::window& win() { return _win; }
  ntfr::context_view ctx() const { return _ctx; }

public:
  void render_loop(auto&& fun) {
    ntfr::render_loop(_win, _ctx, GAME_UPS, std::forward<decltype(fun)>(fun));
  }

private:
  ntfr::window _win;
  ntfr::context _ctx;
  vert_shader_array _vert_shaders;
};
