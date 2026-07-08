#pragma once

#include "core.hpp"

#include <vulkan/vulkan_core.h>

#include <chrono>

typedef struct GLFWwindow GLFWwindow;

namespace kappa::render {

class GLFWImGuiHandler {
private:
  explicit GLFWImGuiHandler(GLFWwindow* win) noexcept;

public:
  void init();
  void new_frame();
  void destroy();

private:
  GLFWwindow* _win;
  friend class GLFWContext;
};

class GLFWExtentUpdater {
private:
  explicit GLFWExtentUpdater(GLFWwindow* win) noexcept;

public:
  fn update_extent(VkExtent2D* ext) -> void;

  fn operator()(VkExtent2D* ext)->void { update_extent(ext); }

private:
  GLFWwindow* _win;
  friend class GLFWContext;
};

class GLFWSurfaceCreator {
private:
  explicit GLFWSurfaceCreator(GLFWwindow* win) noexcept;

public:
  fn create_surface(VkInstance vk, VkSurfaceKHR* surface, const VkAllocationCallbacks* vkalloc)
    -> VkResult;

  fn operator()(VkInstance vk, VkSurfaceKHR* surface, const VkAllocationCallbacks* vkalloc)
    ->VkResult {
    return create_surface(vk, surface, vkalloc);
  }

private:
  GLFWwindow* _win;
  friend class GLFWContext;
};

class GLFWContext {
private:
  struct create_t {};

public:
  static fn get_surface_extensions() -> Span<const char*>;

public:
  GLFWContext(create_t, GLFWwindow* win) noexcept;
  ~GLFWContext();

public:
  static fn initialize(TypeBufferRef<GLFWContext> glfw, u32 width, u32 height) -> void;

public:
  static fn fb_resize_fn(GLFWwindow* win, int w, int h) -> void;

public:
  fn make_vk_extent_updater() const -> GLFWExtentUpdater;
  fn make_vk_surface_creator() const -> GLFWSurfaceCreator;
  fn make_imgui_handler() const -> GLFWImGuiHandler;

public:
  fn poll_events() -> void;
  fn should_close() -> bool;

private:
  GLFWwindow* _win;
};

namespace meta {

template<typename F>
concept delta_render_func = std::invocable<F, f64>; // f(dt) -> void

template<typename T>
concept delta_render_object = requires(T obj, f64 delta_time) {
  { obj.on_render(delta_time) } -> std::same_as<void>;
};

template<typename F>
concept fixed_render_func = std::invocable<F, f64, f64>; // f(dt, alpha) -> void

template<typename F, u32 UPS>
concept fixed_update_func =
  std::invocable<F, u32> || std::invocable<F, std::integral_constant<u32, UPS>>; // f(ups) -> void

template<typename T>
concept fixed_render_object = requires(T obj, f64 delta_time, f64 alpha) {
  { obj.on_render(delta_time, alpha) } -> std::convertible_to<void>;
};

template<typename T, u32 UPS>
concept fixed_update_object = requires(T obj, u32 fixed_delta) {
  { obj.on_fixed_update(fixed_delta) } -> std::same_as<void>;
} || requires(T obj) {
  { obj.on_fixed_update(std::integral_constant<u32, UPS>{}) } -> std::same_as<void>;
};

template<typename T, u32 UPS>
concept fixed_loop_object = (fixed_render_func<T> && fixed_update_func<T, UPS>) ||
                            (fixed_render_object<T> && fixed_update_object<T, UPS>);

template<typename T>
concept delta_loop_object = delta_render_func<T> || delta_render_object<T>;

} // namespace meta

template<meta::delta_loop_object LoopObj>
void render_loop(GLFWContext& win, LoopObj&& obj) {
  using namespace std::literals;

  using clock = std::chrono::steady_clock;
  using duration = clock::duration;
  using time_point = std::chrono::time_point<clock, duration>;

  time_point last_time = clock::now();
  while (!win.should_close()) {
    const time_point start_time = clock::now();
    const auto elapsed_time = start_time - last_time;
    last_time = start_time;
    const f64 dt = (std::chrono::duration<f64>(elapsed_time) / 1s);

    win.poll_events();
    if constexpr (meta::delta_render_object<LoopObj>) {
      obj.on_render(dt);
    } else {
      obj(dt);
    }
  }
};

template<u32 UPS, meta::fixed_loop_object<UPS> LoopObj>
void render_loop(GLFWContext& win, LoopObj&& obj) {
  using namespace std::literals;

  static constexpr std::chrono::duration<f64> fixed_elapsed_time =
    std::chrono::microseconds(1000000 / UPS);

  using clock = std::chrono::steady_clock;
  using duration = decltype(clock::duration{} + fixed_elapsed_time);
  using time_point = std::chrono::time_point<clock, duration>;

  time_point last_time = clock::now();
  duration lag = 0s;
  while (!win.should_close()) {
    const time_point start_time = clock::now();
    const auto elapsed_time = start_time - last_time;
    last_time = start_time;
    lag += elapsed_time;

    const f64 dt = (std::chrono::duration<f64>(elapsed_time) / 1s);
    const f64 alpha = (std::chrono::duration<f64>(lag) / fixed_elapsed_time);

    win.poll_events();

    while (lag >= fixed_elapsed_time) {
      if constexpr (meta::fixed_update_object<LoopObj, UPS>) {
        obj.on_fixed_update(std::integral_constant<u32, UPS>{});
      } else {
        obj(std::integral_constant<u32, UPS>{});
      }
      lag -= fixed_elapsed_time;
    }

    if constexpr (meta::fixed_render_object<LoopObj>) {
      obj.on_render(dt, alpha);
    } else {
      obj(dt, alpha);
    }
  }
}

} // namespace kappa::render
