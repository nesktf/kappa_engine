#pragma once

#include <shogle/boilerplate.hpp>

#include <ntfstl/singleton.hpp>
#include <variant>

using namespace ntf::numdefs;

using ntf::mat4;
using ntf::vec2;
using ntf::vec3;
using ntf::vec4;
using ntf::ivec4;

enum vertex_stage_flags {
  VERTEX_STAGE_NO_FLAGS = 0,
  VERTEX_STAGE_HAS_SCENE_TRANSFORMS = 1 << 0,
  VERTEX_STAGE_EXPORTS_TANGENTS = 1 << 1,
  VERTEX_STAGE_EXPORTS_NORMALS = 1 << 2,
  VERTEX_STAGE_EXPORTS_CUBEMAP_UVS = 1 << 3,

  VERTEX_STAGE_MODEL_NONE = 0 << 4, // Has no model uniform
  VERTEX_STAGE_MODEL_MATRIX = 1 << 4, // Has a single matrix as model uniform
  VERTEX_STAGE_MODEL_ARRAY = 2 << 4, // Has an array of matrices as model uniform
  VERTEX_STAGE_MODEL_OFFSET = 3 << 4, // Has a matrix and a vec4 with offsets as model uniform
};

struct vertex_stage_props {
  std::vector<ntfr::attribute_binding> att_bindings;
  ntfr::shader_t shader;
  u32 flags;
};

class pipeline_provider {
public:
  enum vert_type {
    VERT_RIGGED_MODEL = 0,
    VERT_STATIC_MODEL,
    VERT_GENERIC_MODEL,
    VERT_SKYBOX,
    VERT_SPRITE,
    VERT_EFFECT,

    VERT_COUNT,
  };

public:
  pipeline_provider(std::array<ntfr::vertex_shader, VERT_COUNT>&& vert_shaders);

public:
  static pipeline_provider create(ntfr::context_view ctx);

public:
  vertex_stage_props make_vert_stage(vert_type type, bool aos_bindings);

public:
  std::array<ntfr::vertex_shader, VERT_COUNT> _vert_shaders;
};

class renderer : public ntf::singleton<renderer> {
public:
  static constexpr u32 GAME_UPS = 60u;

private:
  friend ntf::singleton<renderer>;

  struct handle_t {
    ~handle_t() { renderer::destroy(); }
  };

private:
  renderer(ntfr::window&& win, ntfr::context&& ctx,
           pipeline_provider&& pip_prov);

public:
  static handle_t construct();

public:
  ntfr::window& win() { return _win; }
  ntfr::context_view ctx() const { return _ctx; }

public:
  vertex_stage_props make_vert_stage(pipeline_provider::vert_type type, bool aos_bindings = false);

public:
  void render_loop(auto&& fun) {
    ntfr::render_loop(_win, _ctx, GAME_UPS, std::forward<decltype(fun)>(fun));
  }

private:
  ntfr::window _win;
  ntfr::context _ctx;
  pipeline_provider _pip_prov;
};
