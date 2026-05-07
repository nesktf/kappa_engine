#pragma once

#include "./vk_private.hpp"

namespace kappa::render {

fn vk_draw_imgui(VkCommandBuffer cmd, VkImageView target, VkExtent2D swapchain_extent) -> void;
fn vk_imgui_new_frame() -> void;
fn vk_init_imgui(VulkanContext_impl& ctx, const VulkanSurfaceProvider::ImGuiFn& imgui_init)
  -> void;

} // namespace kappa::render
