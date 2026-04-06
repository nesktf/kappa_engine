#pragma once

#include "../util/freelist.hpp"
#include "../util/ptr.hpp"

#include "./vk_device.hpp"
#include "./vk_swapchain.hpp"

namespace kappa::render {

constexpr usize MAX_FRAMES_IN_FLIGHT = 2;

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
