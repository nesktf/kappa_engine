#pragma once

#include "./vk_private.hpp"

#include "../../util/array.hpp"

namespace kappa::render {

class VulkanDevice {
private:
  struct create_t {};

public:
  struct QueueIndices {
    u32 graphics, present, transfer;
  };

public:
  VulkanDevice(create_t, VkDevice device, VkPhysicalDevice physical_device, QueueIndices queues,
               Vec<VkSurfaceFormatKHR>&& surface_formats,
               Vec<VkPresentModeKHR>&& surface_present_modes);

public:
  static fn create(VkInstance vk, VkSurfaceKHR surface) -> VkExpect<VulkanDevice>;

public:
  fn add_to_delqueue(VulkanDelQueue& queue) -> void;

  fn wait_idle() -> void;

public:
  VkDevice device() const { return _device; }

  VkPhysicalDevice physical_device() const { return _physical_device; }

  QueueIndices queues() const { return _queues; }

  Span<const VkSurfaceFormatKHR> surface_formats() const {
    return {_surface_formats.data(), _surface_formats.size()};
  }

  Span<const VkPresentModeKHR> surface_present_modes() const {
    return {_surface_present_modes.data(), _surface_present_modes.size()};
  }

private:
  VkDevice _device;
  VkPhysicalDevice _physical_device;
  QueueIndices _queues;
  Vec<VkSurfaceFormatKHR> _surface_formats;
  Vec<VkPresentModeKHR> _surface_present_modes;
};

} // namespace kappa::render
