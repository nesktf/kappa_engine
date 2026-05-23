#pragma once

#include "./vk_common.hpp"

#include "../../util/function.hpp"

#include <imgui.h>

namespace kappa::render {

struct VkImGuiInfo {
  VkInstance vk;
  VkPhysicalDevice physical_device;
  VkDevice device;
  VkQueue queue;
  VkFormat swapchain_format;
  VkDescriptorPool descpool;
};

using VkImGuiInitFn = FnRef<void()>;

fn vk_init_imgui_fn(VkContext_Impl& vk, VkImGuiInitFn imgui_initer) -> VkExpect<void>;

template<typename Fn>
fn vk_init_imgui(VkContext_Impl& vk, Fn&& imgui_initer) -> VkExpect<void> {
  return vk_init_imgui_fn(vk, VkImGuiInitFn{imgui_initer});
}

fn vk_draw_imgui(VkCommandBuffer cmd, ImDrawData* draw_data, VkPipeline pipeline = VK_NULL_HANDLE)
  -> void;
fn vk_shutdown_imgui() -> void;

} // namespace kappa::render
