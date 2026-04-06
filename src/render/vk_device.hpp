#pragma once

#include "../util/array.hpp"
#include "../util/ptr.hpp"

#include "./vk_common.hpp"

namespace kappa::render {

class GraphicsDevice {
public:
  struct SwapchainCaps {
    VkView<VkSurfaceKHR> surface;
    Vec<VkSurfaceFormatKHR> formats;
    Vec<VkPresentModeKHR> present_modes;
  };

  enum QueueFamily {
    QUEUE_FAMILY_GRAPHICS = 0,
    QUEUE_FAMILY_PRESENT,
    QUEUE_FAMILY_TRANSFER,

    QUEUE_FAMILY_COUNT,
  };

  struct QueueFamilyIndices {
    u32 graphics;
    u32 present;
    u32 transfer;
  };

public:
  GraphicsDevice(VkView<VkInstance> vk, VkView<VkSurfaceKHR> surface);

  GraphicsDevice(VkPhysicalDevice physical_device, VkDevice device,
                 const QueueFamilyIndices& indices, SwapchainCaps&& caps);

public:
  fn destroy() -> void;

public:
  fn device() const -> VkView<VkDevice>;
  fn physical_device() const -> VkView<VkPhysicalDevice>;

  fn swapchain_formats() const -> Span<const VkSurfaceFormatKHR>;
  fn swapchain_present_modes() const -> Span<const VkPresentModeKHR>;
  fn swapchain_capabilities() const -> VkSurfaceCapabilitiesKHR;

  fn queue_families() const -> QueueFamilyIndices;

  fn get_queue(QueueFamily family, u32 queue_index = 0u) const -> VkView<VkQueue>;

  fn physical_device_props() const -> VkPhysicalDeviceProperties;

public:
  operator VkDevice() const { return _device; }

  operator VkPhysicalDevice() const { return _physical_device; }

private:
  VkPhysicalDevice _physical_device;
  VkDevice _device;
  QueueFamilyIndices _family_indices;
  SwapchainCaps _swapchain_capabilities;
};

} // namespace kappa::render
