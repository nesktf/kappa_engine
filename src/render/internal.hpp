#pragma once

#include "./instance.hpp"

namespace kappa::render {

struct render_context {
public:
  render_context(shogle::gl_context&& gl_, shogle::glfw_win&& win_,
                 shogle::gl_pipeline&& pipeline_);

public:
  shogle::gl_context gl;
  shogle::glfw_win win;
  shogle::gl_pipeline pipeline;
  std::vector<shogle::gl_pipeline> pipelines;
  m4f32 proj{1.f};
};

shogle::gl_s_expect<shogle::gl_pipeline> initialize_pipeline();

} // namespace kappa::render
