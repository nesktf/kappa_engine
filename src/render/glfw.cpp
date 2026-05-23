#include "./glfw.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>

namespace kappa::render {

GLFWContext::GLFWContext(GLFWwindow* win) : _win(win) {}

fn GLFWContext::create(u32 width, u32 height) -> GLFWContext {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  auto res = glfwInit();
  ka_assert(res, "Failed to initialize GLFW");

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* win = glfwCreateWindow(width, height, "test", nullptr, nullptr);
  ka_assert(win, "Failed to create GLFW window");
  return {win};
}

fn GLFWContext::fb_resize_fn(GLFWwindow* win, int w, int h) -> void {
  auto& self = *static_cast<GLFWContext*>(glfwGetWindowUserPointer(win));
  KA_UNUSED(self);
  KA_UNUSED(w);
  KA_UNUSED(h);
  // vk_rebuild_swapchain(*self._vk, VkExtent2D(w, h));
}

fn GLFWContext::destroy() -> void {
  glfwDestroyWindow(_win);
  glfwTerminate();
}

fn GLFWContext::ImGuiIniter::operator()() -> void {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

  ImGui_ImplGlfw_InitForVulkan(_win, true);
}

fn GLFWContext::ImGuiIniter::destroy() -> void {
  ImGui_ImplGlfw_Shutdown();
}

fn GLFWContext::make_imgui_initer() -> ImGuiIniter {
  return ImGuiIniter{_win};
}

fn GLFWContext::start_imgui_frame() -> void {
  ImGui_ImplGlfw_NewFrame();
}

fn GLFWContext::poll_events() -> void {
  glfwPollEvents();
}

fn GLFWContext::should_close() -> bool {
  return glfwWindowShouldClose(_win);
}

} // namespace kappa::render
