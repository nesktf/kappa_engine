#pragma once

#include "./vk_util.hpp"

#include "../../util/array.hpp"
#include "../../util/ptr.hpp"

namespace kappa::render {

class VkContextDevice {
private:
  struct create_t {};

public:
  struct QueueIndices {
    u32 graphics, present, transfer;
  };

public:
  VkContextDevice(create_t, VkDevice device, VkPhysicalDevice physical_device, QueueIndices queues,
                  Vec<VkSurfaceFormatKHR>&& surface_formats,
                  Vec<VkPresentModeKHR>&& surface_present_modes);

public:
  static fn create(VkInstance vk, VkSurfaceKHR surface) -> VkExpect<VkContextDevice>;

public:
  fn add_to_delqueue(VkDelQueue& queue) -> void;

  fn wait_idle() -> void;

public:
  fn device() const -> VkDevice { return _device; }

  fn physical_device() const -> VkPhysicalDevice { return _physical_device; }

  fn queues() const -> QueueIndices { return _queues; }

  fn surface_formats() const -> Span<const VkSurfaceFormatKHR> {
    return {_surface_formats.data(), _surface_formats.size()};
  }

  fn surface_present_modes() const -> Span<const VkPresentModeKHR> {
    return {_surface_present_modes.data(), _surface_present_modes.size()};
  }

  fn graphics_queue(u32 idx = 0) const -> VkQueue;
  fn present_queue(u32 idx = 0) const -> VkQueue;
  fn transfer_queue(u32 idx = 0) const -> VkQueue;

private:
  VkDevice _device;
  VkPhysicalDevice _physical_device;
  QueueIndices _queues;
  Vec<VkSurfaceFormatKHR> _surface_formats;
  Vec<VkPresentModeKHR> _surface_present_modes;
};

} // namespace kappa::render
