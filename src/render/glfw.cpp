#include "./glfw.hpp"

#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>

namespace kappa::render {

GLFWContext::GLFWContext(create_t, GLFWwindow* win) noexcept : _win(win) {}

GLFWContext::~GLFWContext() {
  glfwDestroyWindow(_win);
  glfwTerminate();
}

fn GLFWContext::initialize(TypeBufferRef<GLFWContext> glfw, u32 width, u32 height) -> void {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  auto res = glfwInit();
  ka_assert(res, "Failed to initialize GLFW");

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* win = glfwCreateWindow(width, height, "test", nullptr, nullptr);
  ka_assert(win, "Failed to create GLFW window");
  glfw->construct(create_t(), win);
}

fn GLFWContext::fb_resize_fn(GLFWwindow* win, int w, int h) -> void {
  auto& self = *static_cast<GLFWContext*>(glfwGetWindowUserPointer(win));
  KA_UNUSED(self);
  KA_UNUSED(w);
  KA_UNUSED(h);
}

GLFWExtentUpdater::GLFWExtentUpdater(GLFWwindow* win) noexcept : _win(win) {}

fn GLFWExtentUpdater::update_extent(VkExtent2D* ext) -> void {
  int w, h;
  glfwGetFramebufferSize(_win, &w, &h);
  ext->width = (u32)w;
  ext->height = (u32)h;
}

fn GLFWContext::make_vk_extent_updater() const -> GLFWExtentUpdater {
  return GLFWExtentUpdater{_win};
}

GLFWSurfaceCreator::GLFWSurfaceCreator(GLFWwindow* win) noexcept : _win(win) {}

fn GLFWSurfaceCreator::create_surface(VkInstance vk, VkSurfaceKHR* surface,
                                      const VkAllocationCallbacks* vkalloc) -> VkResult {
  return glfwCreateWindowSurface(vk, _win, vkalloc, surface);
}

fn GLFWContext::make_vk_surface_creator() const -> GLFWSurfaceCreator {
  return GLFWSurfaceCreator{_win};
}

GLFWImGuiHandler::GLFWImGuiHandler(GLFWwindow* win) noexcept : _win(win) {}

fn GLFWImGuiHandler::init() -> void {
  ImGui_ImplGlfw_InitForVulkan(_win, true);
}

fn GLFWImGuiHandler::destroy() -> void {
  ImGui_ImplGlfw_Shutdown();
}

fn GLFWImGuiHandler::new_frame() -> void {
  ImGui_ImplGlfw_NewFrame();
}

fn GLFWContext::make_imgui_handler() const -> GLFWImGuiHandler {
  return GLFWImGuiHandler{_win};
}

fn GLFWContext::poll_events() -> void {
  glfwPollEvents();
}

fn GLFWContext::should_close() -> bool {
  return glfwWindowShouldClose(_win);
}

fn GLFWContext::get_surface_extensions() -> Span<const char*> {
  u32 glfw_ext_count = 0;
  const char** glfw_exts_ptr = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
  return {glfw_exts_ptr, (size_t)glfw_ext_count};
}

} // namespace kappa::render
