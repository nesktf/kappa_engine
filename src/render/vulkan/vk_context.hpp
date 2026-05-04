#pragma once

#include "./vk_private.hpp"

#include "./vk_buffer.hpp"
#include "./vk_device.hpp"
#include "./vk_pipeline.hpp"
#include "./vk_swapchain.hpp"
#include "./vk_util.hpp"

namespace kappa::render {

// Shitty trivial nullable
template<typename T>
class PartialInit {
public:
  PartialInit() = default;

  ~PartialInit() { reset(); }

public:
  template<typename... Args>
  T& emplace(Args&&... args) {
    reset();
    auto* ptr = new (reinterpret_cast<T*>(_buffer)) T(std::forward<Args>(args)...);
    _buffer[sizeof(T)] = 1;
    return *ptr;
  }

  void reset() {
    if (*this) {
      if constexpr (!std::is_trivially_destructible_v<T>) {
        reinterpret_cast<T*>(_buffer)->~T();
      }
      _buffer[sizeof(T)] = 0;
    }
  }

  const T& get() const {
    ka_assert(_buffer[sizeof(T)] == 1, "Not initialized");
    return *reinterpret_cast<const T*>(_buffer);
  }

  T& get() { return const_cast<T&>(std::as_const(*this).get()); }

public:
  T& operator*() { return get(); }

  const T& operator*() const { return get(); }

  T* operator->() { return &get(); }

  const T* operator->() const { return &get(); }

  explicit operator bool() const { return _buffer[sizeof(T)] == 1; }

private:
  alignas(T) u8 _buffer[sizeof(T) + 1];
};

struct VulkanContextImpl {
public:
  struct FrameData {
    VkCommandPool cmdpool;
    VkCommandBuffer cmdbuf;
    VkSemaphore swapchain_sem, render_sem;
    VkFence render_fen;
  };

  // temp
  struct DrawThing {
    VulkanImage image;
    VkExtent2D extent;
    VulkanDescPool desc_pool;
    VkDescriptorSet image_desc;
    VkDescriptorSetLayout image_desc_layout;
    VkPipeline gradient_pipeline;
    VkPipelineLayout gradient_layout;
  };

  struct ImDrawData {
    VkFence fence;
    VkCommandPool cmdpool;
    VkCommandBuffer cmdbuf;
  };

public:
  VulkanContextImpl() = default; // Manually initialized

  VulkanContextImpl(VulkanContextImpl&&) = delete;
  VulkanContextImpl(const VulkanContextImpl&) = delete;
  VulkanContextImpl& operator=(VulkanContextImpl&&) = delete;
  VulkanContextImpl& operator=(const VulkanContextImpl&) = delete;
  ~VulkanContextImpl() = default;

public:
  VkInstance vk;
  VkDebugUtilsMessengerEXT messenger;
  VmaAllocator vmalloc;
  VkSurfaceKHR surface;
  VulkanDelQueue delqueue;

  PartialInit<VulkanDevice> device;
  PartialInit<VulkanSwapchain> swapchain;

  PartialInit<FrameData> frames[MAX_FRAMES_IN_FLIGHT];
  u32 curr_frame;

  PartialInit<DrawThing> draw;
  PartialInit<ImDrawData> imdraw;
};

fn vk_get_graphics_queue(VulkanContextImpl& ctx) -> VkQueue;
fn vk_draw_imgui(VkCommandBuffer cmd, VkImageView target, VkExtent2D swapchain_extent) -> void;

} // namespace kappa::render
