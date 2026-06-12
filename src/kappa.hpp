#pragma once

#include "./render/context.hpp"
#include "./render/glfw.hpp"
#include "./render/scene.hpp"

#include "./util/buffer.hpp"

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
  TypeBufferFor<render::GLFWContext> _glfw;
  TypeBufferFor<render::RenderContext> _renderer;
  TypeBufferFor<render::SceneData> _scene;
  bool _initialized = false;
};

} // namespace kappa
