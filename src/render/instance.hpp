#pragma once

#include "../core.hpp"

#include <shogle/render/opengl.hpp>
#include <shogle/render/window.hpp>

namespace kappa::render {

void initialize(u32 win_w, u32 win_h);
void destroy();

void start_frame();
void end_frame();

shogle::glfw_win& window();

} // namespace kappa::render
