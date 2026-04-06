#pragma once

#include "../util/freelist.hpp"
#include "./instance.hpp"

namespace kappa::render {

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

} // namespace kappa::render
