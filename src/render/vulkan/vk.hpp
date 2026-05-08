#pragma once

#include "./vk.h"

#include "../../util/expected.hpp"
#include "../../util/function.hpp"
#include "../../util/ptr.hpp"

#include <type_traits>

namespace kappa::render {

class VkError : public std::exception {
public:
  VkError(VkResult err) : _err(err) {}

public:
  fn what() const noexcept -> const char* override { return ka_vk_error_str(_err); }

  fn code() const noexcept -> VkResult { return _err; }

private:
  VkResult _err;
};

template<typename T>
using VkExpect = Expected<T, VkError>;

class VkMsgError : public std::exception {
public:
  VkMsgError(const char* msg, VkResult err) noexcept : _msg(msg), _err(err) {}

public:
  fn what() const noexcept -> const char* override { return _msg; }

  fn code() const noexcept -> VkResult { return _err; }

private:
  const char* _msg;
  VkResult _err;
};

template<typename T>
using VkExpect = Expected<T, VkError>;

template<typename T>
using VkMsgExpect = Expected<T, VkMsgError>;

struct VulkanContextArgs {
public:
  using ImGuiInitFn = FnRef<void()>;
  using SurfaceCreateFn =
    FnRef<VkResult(VkInstance vk, VkSurfaceKHR* surface, const VkAllocationCallbacks* vkalloc)>;

public:
  VkExtent2D initial_extent;
  Span<const char*> surface_extensions;
  SurfaceCreateFn create_surface_fn;
  Optional<ImGuiInitFn> init_imgui_fn;
  const char* app_name;
  u32 app_ver;
};

class VulkanContext {
private:
  struct Deleter {
    void operator()(ka_VulkanContext ctx) noexcept { ka_vk_destroy_context(ctx); }
  };

  struct create_t {};

public:
  using ImSubmitFn = TrivFn<void(VkCommandBuffer), 4 * sizeof(void*), 8>;

public:
  VulkanContext(create_t, ka_VulkanContext ctx) noexcept : _ctx(ctx) {}

public:
  static fn create(const VulkanContextArgs& args) noexcept -> VkMsgExpect<VulkanContext>;

public:
  fn rebuild_swapchain(VkExtent2D surface_extent) noexcept -> VkExpect<void>;

  fn draw() noexcept -> VkExpect<void>;

  template<typename F>
  fn immediate_submit(F&& func) -> VkExpect<void>;

public:
  operator ka_VulkanContext() noexcept { return _ctx.get(); }

private:
  std::unique_ptr<std::remove_pointer_t<ka_VulkanContext>, Deleter> _ctx;
};

using VulkanImageArgs = ka_VulkanImageArgs;

class VulkanImage : public ka_VulkanImage {
private:
  struct create_t {};

public:
  VulkanImage(create_t, ka_VulkanImage&& image) noexcept : ka_VulkanImage(image) {}

public:
  static fn allocate(ka_VulkanContext vk, const VulkanImageArgs& args) -> VkExpect<VulkanImage>;
  fn destroy() -> void;

public:
  fn extent() -> VkExtent2D;
  fn format() -> VkFormat;
};

using VulkanBufferArgs = ka_VulkanBufferArgs;

class VulkanBuffer : public ka_VulkanBuffer {
private:
  struct create_t {};

public:
  VulkanBuffer(create_t, ka_VulkanBuffer&& buffer) noexcept : ka_VulkanBuffer(buffer) {}

public:
  static fn allocate(ka_VulkanContext vk, const VulkanBufferArgs& args) -> VkExpect<VulkanBuffer>;
  fn destroy() -> void;

public:
  fn size() -> usize;
  fn mapped_data() -> void*;
};

class VulkanShaderModule : public ka_VulkanShaderModule {
public:
  struct create_t {};

public:
  VulkanShaderModule(create_t, ka_VulkanShaderModule&& shader_module) noexcept :
      ka_VulkanShaderModule(shader_module) {}

public:
  static fn create(ka_VulkanContext vk, Span<const u8> src, VkShaderStageFlags stage)
    -> VkExpect<VulkanShaderModule>;
  fn destroy() -> void;
};

class VulkanCompute : public ka_VulkanCompute {};

class VulkanPipeline : public ka_VulkanPipeline {};

class VulkanPipelineBuilder {
private:
  struct Deleter {
    void operator()(ka_VulkanPipelineBuilder pipb) noexcept {
      ka_vk_destroy_pipeline_builder(pipb);
    }
  };

  struct create_t {};

public:
  VulkanPipelineBuilder(create_t, ka_VulkanPipelineBuilder pipb) noexcept : _pipb(pipb) {}

public:
  static fn create() -> VulkanPipelineBuilder;

public:
  fn build(ka_VulkanContext vk) -> VkExpect<VulkanPipeline>;
  fn clear() -> void;

public:
  fn add_shader(const VulkanShaderModule& shader) -> VulkanPipelineBuilder&;

  fn set_layout(VkPipelineLayout layout) -> VulkanPipelineBuilder&;
  fn set_topology(VkPrimitiveTopology topology) -> VulkanPipelineBuilder&;
  fn set_poly_mode(VkPolygonMode mode, f32 width = 1.f) -> VulkanPipelineBuilder&;
  fn set_cull_mode(VkCullModeFlags mode, VkFrontFace front_face) -> VulkanPipelineBuilder&;
  fn set_color_format(VkFormat format) -> VulkanPipelineBuilder&;
  fn set_depth_format(VkFormat format) -> VulkanPipelineBuilder&;

  fn disable_multisampling() -> VulkanPipelineBuilder&;
  fn disable_blending() -> VulkanPipelineBuilder&;
  fn disable_depth_test() -> VulkanPipelineBuilder&;

public:
  operator ka_VulkanPipelineBuilder() noexcept { return _pipb.get(); }

private:
  std::unique_ptr<std::remove_pointer_t<ka_VulkanPipelineBuilder>, Deleter> _pipb;
};

} // namespace kappa::render

namespace kappa::render {

inline fn VulkanContext::create(const VulkanContextArgs& args) noexcept
  -> VkMsgExpect<VulkanContext> {
  ka_VulkanContextArgs c_args{};
  c_args.app_name = args.app_name;
  c_args.app_ver = args.app_ver;

  fn create_surface = [&](VkInstance vk, VkSurfaceKHR* surface,
                          const VkAllocationCallbacks* vkalloc) -> VkResult {
    return args.create_surface_fn(vk, surface, vkalloc);
  };
  const fn create_surface_trampoline = +[](void* user, VkInstance vk, VkSurfaceKHR* surface,
                                           const VkAllocationCallbacks* vkalloc) -> VkResult {
    return (*(decltype(create_surface)*)user)(vk, surface, vkalloc);
  };
  c_args.create_surface = create_surface_trampoline;
  c_args.create_surface_user = &create_surface;
  c_args.surface_extensions = args.surface_extensions.data();
  c_args.surface_extension_count = args.surface_extensions.size();

  fn init_imgui = [&]() -> void {
    (*args.init_imgui_fn)();
  };
  const fn init_imgui_trampoline = +[](void* user) -> void {
    (*(decltype(init_imgui)*)user)();
  };
  if (args.init_imgui_fn) {
    c_args.init_imgui = init_imgui_trampoline;
    c_args.init_imgui_user = &init_imgui;
  }

  ka_VulkanContext ctx;
  const char* msg;
  if (auto ret = ka_vk_create_context(&ctx, &c_args, &msg)) {
    return {unexpect, msg, ret};
  }
  return {in_place, create_t(), ctx};
}

inline fn VulkanContext::rebuild_swapchain(VkExtent2D surface_extent) noexcept -> VkExpect<void> {
  if (auto ret = ka_vk_rebuild_swapchain(_ctx.get(), surface_extent)) {
    return {unexpect, ret};
  }
  return {};
}

inline fn VulkanContext::draw() noexcept -> VkExpect<void> {
  if (auto ret = ka_vk_draw(_ctx.get())) {
    return {unexpect, ret};
  }
  return {};
}

template<typename F>
fn VulkanContext::immediate_submit(F&& func) -> VkExpect<void> {
  static_assert(std::is_invocable_v<std::remove_cvref_t<F>, VkCommandBuffer>,
                "F is not invocable");
  ka_VulkanImSubmitData c_data{};

  fn submit = [&](VkCommandBuffer cmd) {
    func(cmd);
  };
  const fn submit_trampoline = +[](void* user, VkCommandBuffer cmd) -> void {
    (*(decltype(submit)*)user)(cmd);
  };
  c_data.submit_callback_user = &submit;
  c_data.submit_callback = submit_trampoline;

  if (auto ret = ka_vk_immediate_submit(_ctx.get(), &c_data)) {
    return {unexpect, ret};
  }
  return {};
}

} // namespace kappa::render
