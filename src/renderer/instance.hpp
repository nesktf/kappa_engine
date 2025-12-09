#pragma once

#include "./context.hpp"
#include "./shaders.hpp"

namespace kappa::render {

using vert_shader_array = std::array<shogle::vertex_shader, VERT_SHADER_COUNT>;

struct render_ctx {
  render_ctx(shogle::window&& win_, shogle::context&& ctx_, shogle::texture2d&& missing_tex_,
             vert_shader_array&& vert_shaders_);

  shogle::window win;
  shogle::context ctx;
  shogle::texture2d missing_tex;
  vert_shader_array vert_shaders;
  object_render_data render_data;
};

extern ntf::nullable<render_ctx> g_renderer;

enum vertex_stage_flags {
  VERTEX_STAGE_NO_FLAGS = 0,

  VERTEX_STAGE_SCENE_TRANSFORMS = 1 << 0,

  VERTEX_STAGE_EXPORTS_TANGENTS = 1 << 1,
  VERTEX_STAGE_EXPORTS_NORMALS = 1 << 2,
  VERTEX_STAGE_EXPORTS_CUBEMAP_UVS = 1 << 3,

  VERTEX_STAGE_MODEL_NONE = 0 << 4,   // Has no model uniform
  VERTEX_STAGE_MODEL_MATRIX = 1 << 4, // Has a single matrix as model uniform
  VERTEX_STAGE_MODEL_ARRAY = 2 << 4,  // Has an array of matrices as model uniform
  VERTEX_STAGE_MODEL_OFFSET = 3 << 4, // Has a matrix and a vec4 with offsets as model uniform
};

enum fragment_stage_flags {
  FRAGMENT_STAGE_NO_FLAGS = 0,

  FRAGMENT_STAGE_TANGENTS = 1 << 0,
  FRAGMENT_STAGE_NORMALS = 1 << 1,

  FRAGMENT_STAGE_SAMPLER_COUNT = 3 << 2,
};

enum fragment_sampler_type {
  FRAGMENT_SAMPLER_ALBEDO = 0, // We use albedo and diffuse interchangeably
  FRAGMENT_SAMPLER_SPECULAR,
  FRAGMENT_SAMPLER_NORMALS,
  FRAGMENT_SAMPLER_DISPLACEMENT,

  FRAGMENT_SAMPLER_COUNT,
};

vert_shader_array initialize_shaders(shogle::context_view ctx);

expect<shogle::fragment_shader> make_frag_stage(shogle::context_view ctx, frag_shader_type type,
                                                u32 vert_flags);

shogle::vertex_shader_view make_vert_stage(vert_shader_type type, u32& flags,
                                           std::vector<shogle::attribute_binding>& bindings,
                                           vert_shader_array& verts, bool aos_bindings);

} // namespace kappa::render
