#pragma once

#include "../util/freelist.hpp"
#include "../util/ptr.hpp"

#include "./vk_device.hpp"
#include "./vk_swapchain.hpp"

namespace kappa::render {

constexpr usize MAX_FRAMES_IN_FLIGHT = 2;

struct ISurfaceProvider {
  virtual ~ISurfaceProvider() = default;
  virtual fn get_extensions(Vec<const char*> extensions) -> void;
  virtual fn create_surface(VkInstance vk, VkSurfaceKHR* surface) -> bool;
  virtual fn get_surface_extent() -> VkExtent2D;
};

class VulkanContext;

struct VulkanAppInfo {};

enum class VulkanCommandType {
  transfer = 0,
  graphics,
};

class VulkanCommandHandler {
private:
  struct create_t {};

  friend VulkanContext;

public:
  VulkanCommandHandler(create_t, VulkanContext& ctx, VulkanCommandType type, VkCommandPool cmdpool,
                       UniqueArray<VkCommandBuffer>&& cmdbufs) :
      _ctx(ctx), _cmdpool(cmdpool), _cmdbufs(std::move(cmdbufs)), _type(type) {
    ka_assert(_cmdpool != VK_NULL_HANDLE);
    ka_assert(!_cmdbufs.empty());
  }

  ~VulkanCommandHandler() noexcept;

  VulkanCommandHandler(VulkanCommandHandler&& other) noexcept;
  VulkanCommandHandler(const VulkanCommandHandler&) = delete;

  VulkanCommandHandler& operator=(VulkanCommandHandler&& other) noexcept;
  VulkanCommandHandler& operator=(const VulkanCommandHandler&) = delete;

private:
  RefView<VulkanContext> _ctx;
  VkCommandPool _cmdpool;
  UniqueArray<VkCommandBuffer> _cmdbufs;
  VulkanCommandType _type;
  VkSemaphore _semaphore;
  VkFence _fence;
};

class VulkanContext {
public:
  struct Swapchain {
    VkSwapchainKHR swapchain;
    VkFormat format;
    UniqueArray<VkImage> images;
    UniqueArray<VkImageView> image_views;
    VkExtent2D extent;
    VkSemaphore semaphore;
  };

  struct Device {
    VkPhysicalDevice pdevice;
    VkDevice device;
    VkQueue graphics_queue;
    VkQueue present_queue;
    VkQueue transfer_queue;
    UniqueArray<VkSurfaceFormatKHR> surface_formats;
    UniqueArray<VkPresentModeKHR> surface_present_modes;
  };

public:
  VulkanContext(VkInstance vk, VkDebugUtilsMessengerEXT messenger, VmaAllocator vmalloc,
                Device&& device, VkSurfaceKHR surface, ISurfaceProvider& surface_provider,
                Swapchain&& swapchain) :
      _vk(vk), _messenger(messenger), _vmalloc(vmalloc), _device(std::move(device)),
      _surface(surface), _surface_provider(surface_provider), _swapchain(std::move(swapchain)) {
    ka_assert(_vk != VK_NULL_HANDLE);
    ka_assert(_surface != VK_NULL_HANDLE);
    ka_assert(_vmalloc != VK_NULL_HANDLE);
    ka_assert(_device.pdevice != VK_NULL_HANDLE);
    ka_assert(_device.device != VK_NULL_HANDLE);
    ka_assert(_swapchain.swapchain != VK_NULL_HANDLE);
  }

  ~VulkanContext() noexcept;

  VulkanContext(VulkanContext&& other) noexcept;
  VulkanContext(const VulkanContext&) = delete;

  VulkanContext& operator=(VulkanContext&& other) noexcept;
  VulkanContext& operator=(const VulkanContext& other) noexcept;

public:
  static fn create(const VulkanAppInfo& app_info, ISurfaceProvider& surface_provider)
    -> VkSvExpect<VulkanContext>;

private:
  fn rebuild_swapchain(u32 width, u32 height) -> VkSvExpect<void>;

  fn create_command_handler(VulkanCommandType type, usize cmdbufs)
    -> VkSvExpect<VulkanCommandHandler>;

private:
  VkInstance _vk;
  VkDebugUtilsMessengerEXT _messenger;
  VmaAllocator _vmalloc;
  Device _device;
  VkSurfaceKHR _surface;
  RefView<ISurfaceProvider> _surface_provider;
  Swapchain _swapchain;
};

class RenderContext {
public:
  struct LayoutInfo {
    VkVertexInputBindingDescription bindings;
    Span<const VkVertexInputAttributeDescription> attributes;
  };

  struct GraphicsPipeline {
    VkPipeline pipeline;
    VkPipelineLayout layout;
  };

  static constexpr usize MAX_PIPELINES = 128;

  struct BufferData {
    VkBuffer buffer;
    Nullable<VkDeviceAddress> address;
    VmaAllocation allocation;
    VmaAllocationInfo info;
    VkBufferUsageFlags buffer_usage;
  };

  static constexpr usize MAX_BUFFERS = 128;

  struct CommandPool {
    VkCommandPool graphics;
    VkCommandPool transfer;
    std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> graphics_cmdbufs;
    VkCommandBuffer transfer_cmdbuf;
  };

  struct SyncObjects {
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> image_avail_sem;
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> render_end_sem;
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> in_flight_fences;
  };

public:
  RenderContext(VkInstance vk, VmaAllocator vmalloc, VkSurfaceKHR surface,
                VkDebugUtilsMessengerEXT messenger, GraphicsDevice&& device,
                WindowSwapchain&& swapchain, CommandPool&& cmdpool, SyncObjects sync);

  fn destroy() -> void;

private:
  VkInstance _vk;
  VmaAllocator _vmalloc;
  VkSurfaceKHR _surface;
  VkDebugUtilsMessengerEXT _messenger;
  GraphicsDevice _device;
  WindowSwapchain _swapchain;
  CommandPool _cmdpool;
  SyncObjects _sync;

  u32 _curr_frame;
  u32 _img_index;
  bool _dirty_fb;

  FixedFreelist<GraphicsPipeline, MAX_PIPELINES> pipelines;
  FixedFreelist<BufferData, MAX_BUFFERS> buffers;
};

extern Nullable<RenderContext> g_ctx;

} // namespace kappa::render

#if 0
namespace kappa::render {

constexpr size_t MAX_PIPELINES = 512;
constexpr size_t MAX_TEXTURES = 512;
constexpr size_t MAX_BUFFERS = 512;

constexpr u32 glsl_scene_binding = 1;
constexpr u32 glsl_instance_binding = 2;
constexpr u32 glsl_bone_mat_binding = 3;
constexpr u32 glsl_u_samplers = 1;

struct pipeline_data {
  shogle::gl_pipeline pipeline;
  shogle::gl_vertex_layout layout;
};

struct render_context {
public:
  render_context(shogle::gl_context&& gl_, shogle::glfw_win&& win_,
                 shogle::gl_texture&& default_tex_);

public:
  shogle::glfw_win win;
  shogle::gl_context gl;
  shogle::glfw_imgui imgui;
  shogle::gl_texture default_texture;
  inplace_freelist<pipeline_data, MAX_PIPELINES> pipelines;
  inplace_freelist<shogle::gl_texture, MAX_TEXTURES> textures;
  inplace_freelist<shogle::gl_buffer, MAX_BUFFERS> buffers;
  m4f32 proj{1.f};
};

extern shogle::nullable<render_context> g_ctx;

} // namespace kappa::render
#endif
