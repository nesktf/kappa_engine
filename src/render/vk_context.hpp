#pragma once

#include "./vk_swapchain.hpp"

namespace keiki::render {

struct vk_layout_info {
  VkVertexInputBindingDescription bind;
  span<const VkVertexInputAttributeDescription> attr;
};

struct vk_indexed_draw_command {
  vk_handle pipeline;
  vk_handle vertex_buffer;
  vk_handle index_buffer;
  u32 indices;
};

class vk_context {
public:
  struct vk_memory {
    VmaAllocator vmalloc;
    ptr_view<const VkAllocationCallbacks> vkalloc;
  };

  static constexpr u32 MAX_FRAMES_IN_FLIGHT = 2u;

  template<u32 BuffCount>
  using buffer_array = std::array<VkCommandBuffer, BuffCount>;

  struct command_pool {
    VkCommandPool graphics;
    VkCommandPool transfer;
    buffer_array<MAX_FRAMES_IN_FLIGHT> graphics_command_buffers;
    VkCommandBuffer transfer_command_buffer;
  };

  struct sync_objects {
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> image_avail_semaphores;
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> render_finish_semaphores;
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> in_flight_fences;
  };

  enum frame_flags : u32 {
    FRAME_FLAG_NONE = 0,
    FRAME_FLAG_DIRTY_FRAMEBUFFER = 1 << 0,
  };

public:
  struct graphics_pipeline {
    VkPipeline pipeline;
    VkPipelineLayout layout;
  };

  struct buffer_data {
    VkBuffer buffer;
    ntf::optional<VkDeviceAddress> address;
    VmaAllocation allocation;
    VmaAllocationInfo info;
    VkBufferUsageFlags buffer_usage;
  };

  static constexpr u32 NULL_IMG_INDEX = std::numeric_limits<u32>::max();

public:
  vk_context(scratch_arena&& arena, vk_memory mem, vk_surface_provider& surf_prov, VkInstance vk,
             VkSurfaceKHR surface, VkDebugUtilsMessengerEXT messenger, vk_device&& device,
             vk_swapchain&& swapchain, command_pool&& cmdpool, sync_objects&& sync);

  NTF_DECLARE_NO_MOVE_NO_COPY(vk_context);

public:
  static fn create(size_t arena_size, vk_surface_provider& surf_prov) -> vk_sv_expect<vk_context>;

public:
  fn start_frame() -> vk_sv_expect<void>;
  fn record_command(const vk_indexed_draw_command& cmd) -> void;
  fn end_frame() -> vk_sv_expect<void>;

  fn device_wait() -> void;
  fn flag_dirty_framebuffer() -> void;

public:
  fn create_pipeline(vk_layout_info layout_info, std::string_view vert, std::string_view frag)
    -> vk_sv_expect<vk_handle>;
  fn destroy_pipeline(vk_handle pipeline) -> void;

  fn create_buffer(vk_buffer_type type, size_t size, u32 flags = 0u) -> vk_sv_expect<vk_handle>;
  fn upload_buffer_data(vk_handle buffer, const void* data, size_t size, size_t offset = 0u)
    -> vk_sv_expect<void>;
  fn destroy_buffer(vk_handle buffer) -> void;

private:
  fn _rebuild_swapchain() -> vk_sv_expect<void>;
  fn _current_command_buffer() -> VkCommandBuffer;

  fn _allocate_buffer(VkBufferUsageFlags buffer_usage, VmaMemoryUsage mem_usage, size_t size,
                      u32 flags) -> vk_sv_expect<buffer_data>;
  fn _deallocate_buffer(const buffer_data& data) -> void;

private:
  scratch_arena _arena;
  vk_memory _mem;
  vk_surface_provider& _surf_prov;
  VkInstance _vk;
  VkSurfaceKHR _surface;
  VkDebugUtilsMessengerEXT _messenger;
  vk_device _device;
  vk_swapchain _swapchain;
  command_pool _cmdpool;
  sync_objects _sync;

  u32 _curr_frame;
  u32 _img_index;
  u32 _frame_flags;

  ntf::freelist<graphics_pipeline> _pips;
  ntf::freelist<buffer_data> _buffs;
};

} // namespace keiki::render
