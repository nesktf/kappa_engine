#pragma once

#include "../../util/array.hpp"
#include "../../util/function.hpp"
#include "../../util/ptr.hpp"

#include "./vk_error.hpp"

namespace kappa::render {

struct VulkanInfo {
  const char* app_name;
  u32 app_ver;
};

struct VulkanContextImpl;

struct VulkanDescriptorLayoutBuilder {
  Vec<VkDescriptorSetLayoutBinding> bindings;

  fn add_binding(u32 binding, VkDescriptorType type) -> void;
  fn clear() -> void;
  fn build(VkDevice device, VkShaderStageFlags stages, void* next = nullptr,
           VkDescriptorSetLayoutCreateFlags flags = 0) -> VkDescriptorSetLayout;
};

class VulkanContext {
public:
  using SurfaceProviderFn =
    TrivFn<VkResult(VkInstance, VkSurfaceKHR*, const VkAllocationCallbacks*), 2 * sizeof(void*),
           8>;

  struct Deleter {
    void operator()(VulkanContextImpl* impl) noexcept;
  };

public:
  VulkanContext(VulkanContextImpl& impl) noexcept : _impl(&impl) {}

public:
  static fn create(const VulkanInfo& app_info, VkExtent2D surface_extent,
                   Span<const char*> surface_extensions, SurfaceProviderFn surface_provider)
    -> VkSvExpect<VulkanContext>;

public:
  fn rebuild_swapchain(VkExtent2D surface_extent) -> VkSvExpect<void>;

  fn draw() -> void;

private:
  std::unique_ptr<VulkanContextImpl, Deleter> _impl;
};

} // namespace kappa::render
