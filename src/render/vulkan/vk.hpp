#pragma once

#include "../../util/expected.hpp"
#include "../../util/function.hpp"
#include "../../util/ptr.hpp"

#include <vulkan/vulkan_core.h>

#include <ranmath/ran.hpp>

#define KA_APP_NAME    "Kappa"
#define KA_APP_VERSION VK_MAKE_VERSION(KA_VER_MAJ, KA_VER_MIN, KA_VER_REV)

namespace kappa::render {

fn vk_error_string(VkResult result) noexcept -> const char*;

class VkError : public std::exception {
public:
  VkError(VkResult err) : _err(err) {}

public:
  fn what() const noexcept -> const char* override { return vk_error_string(_err); }

  fn code() const noexcept -> VkResult { return _err; }

private:
  VkResult _err;
};

class VkStrError : public std::exception {
public:
  VkStrError(std::string msg, VkResult err) : _msg(std::move(msg)), _err(err) {}

public:
  fn what() const noexcept -> const char* override { return _msg.c_str(); }

  fn code() const noexcept -> VkResult { return _err; }

  fn code_msg() const noexcept -> std::string_view { return vk_error_string(_err); }

private:
  std::string _msg;
  VkResult _err;
};

class VkSvError : public std::exception {
public:
  VkSvError(const char* msg, VkResult err) noexcept : _msg(msg), _err(err) {}

public:
  fn what() const noexcept -> const char* override { return _msg; }

  fn code() const noexcept -> VkResult { return _err; }

  fn code_msg() const noexcept -> std::string_view { return vk_error_string(_err); }

private:
  const char* _msg;
  VkResult _err;
};

template<typename T>
using VkExpect = Expected<T, VkError>;

template<typename T>
using VkSExpect = Expected<T, VkSvError>;

template<typename T>
using VkSvExpect = Expected<T, VkSvError>;

struct VulkanInfo {
  const char* app_name;
  u32 app_ver;
};

struct ComputeConstants {
  ran::Vec4f32 data1;
  ran::Vec4f32 data2;
  ran::Vec4f32 data3;
  ran::Vec4f32 data4;
};

struct ComputeEffect {
  const char* name;
  VkPipeline pipeline;
  VkPipelineLayout layout;
  ComputeConstants data;
};

struct Vertex {
  ran::Vec3f32 pos;
  f32 uv_x;
  ran::Vec3f32 normal;
  f32 uv_y;
  ran::Vec4f32 color;
};

struct MeshData {
  Span<const u32> indices;
  Span<const Vertex> vertices;
};

struct VulkanContext_impl;

struct VulkanSurfaceProvider {
  using SurfaceProviderFn =
    TrivFn<VkResult(VkInstance, VkSurfaceKHR*, const VkAllocationCallbacks*) const,
           2 * sizeof(void*), 8>;
  using ImGuiFn = TrivFn<void() const, 2 * sizeof(void*), 8>;

  VkExtent2D initial_extent;
  Span<const char*> extensions;
  SurfaceProviderFn provider_fn;
  ImGuiFn imgui_fn;
};

class VulkanContext {
public:
  using ImSubmitFn = TrivFn<void(VkCommandBuffer), 4 * sizeof(void*), 8>;
  using ImGuiDrawFn = TrivFn<void(), 2 * sizeof(void*), 8>;

  struct Deleter {
    void operator()(VulkanContext_impl* impl) noexcept;
  };

public:
  VulkanContext(VulkanContext_impl& impl) noexcept : _impl(&impl) {}

public:
  static fn create(const VulkanInfo& app_info, const VulkanSurfaceProvider& surface_args,
                   const MeshData& mesh) -> VkSvExpect<VulkanContext>;

public:
  fn rebuild_swapchain(VkExtent2D surface_extent) -> VkExpect<void>;

  fn draw(ImGuiDrawFn imgui_draw, const ran::Mat4f32& mesh_transform) -> VkResult;

  fn immediate_submit(ImSubmitFn func) -> void;

public:
  fn get_effect() -> ComputeEffect&;
  fn get_effect_idx() -> s32&;

public:
  fn get() -> VulkanContext_impl* { return _impl.get(); }

private:
  std::unique_ptr<VulkanContext_impl, Deleter> _impl;
};

} // namespace kappa::render
