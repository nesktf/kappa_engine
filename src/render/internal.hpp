#pragma once

#include "../util/freelist.hpp"
#include "./instance.hpp"

namespace kappa::render {

constexpr size_t MAX_PIPELINES = 512;
constexpr size_t MAX_TEXTURES = 512;
constexpr size_t MAX_BUFFERS = 512;

struct render_context {
public:
  render_context(shogle::gl_context&& gl_, shogle::glfw_win&& win_,
                 shogle::gl_pipeline&& pipeline_);

public:
  shogle::gl_context gl;
  shogle::glfw_win win;
  shogle::gl_texture default_texture;
  inplace_freelist<shogle::gl_pipeline, MAX_PIPELINES> pipelines;
  inplace_freelist<shogle::gl_texture, MAX_TEXTURES> textures;
  inplace_freelist<shogle::gl_buffer, MAX_BUFFERS> buffers;
  m4f32 proj{1.f};
};

shogle::gl_s_expect<shogle::gl_pipeline> initialize_pipeline();

} // namespace kappa::render
