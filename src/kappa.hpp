#pragma once

#include "render/context.hpp"
#include "render/glfw.hpp"
#include "render/scene.hpp"

namespace kappa {

class KappaContext {
public:
  KappaContext() = default;

public:
  fn init() -> void;
  fn destroy() -> void;

public:
  fn start() -> void;
  fn on_render(f64 dt, f64 alpha) -> void;
  fn on_fixed_update(u32 ups) -> void;

private:
  TypeBuffer<render::GLFWContext> _glfw;
  TypeBuffer<render::RenderContext> _renderer;
  TypeBuffer<render::SceneData> _scene;
  bool _initialized = false;
};

} // namespace kappa
