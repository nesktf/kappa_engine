#pragma once

#include "./vk_private.hpp"

namespace kappa::render {

fn vk_draw_imgui(VkCommandBuffer cmd, VkImageView target, VkExtent2D swapchain_extent) -> void;
fn vk_init_imgui(VkInstance vk, const VulkanDevice& dev, const VulkanSwapchain& swapchain,
                 VulkanDelQueue& delqueue, PFN_ka_vk_init_imgui init_imgui, void* init_imgui_user)
  -> void;

} // namespace kappa::render
