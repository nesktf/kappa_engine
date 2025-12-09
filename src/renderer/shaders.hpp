#pragma once

#include "../core.hpp"

namespace kappa::render {

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
  shogle::render_tests tests;
  shogle::primitive_mode primitive;
  bool use_aos_bindings;
};

expect<shogle::pipeline> make_pipeline(vert_shader_type vert, frag_shader_type frag,
                                       std::vector<shogle::attribute_binding>& bindings,
                                       const pipeline_opts& opts);

} // namespace kappa::render
