#pragma once

#include "./render/vulkan/vk_buffer.hpp"
#include "./render/vulkan/vk_context.hpp"
#include "./render/vulkan/vk_imgui.hpp"
#include "./render/vulkan/vk_pipeline.hpp"
#include "./render/vulkan/vk_util.hpp"

#include "./glfw.hpp"
#include "assets/ass_common.hpp"

#include <ranmath/ran.hpp>

namespace kappa::render {

class RenderContext {
private:
  struct create_t {};

public:
  struct ComputeConstants {
    ran::Vec4f32 data1;
    ran::Vec4f32 data2;
    ran::Vec4f32 data3;
    ran::Vec4f32 data4;
  };

  struct MeshConstants {
    ran::Mat4f32 world_mat;
    VkDeviceAddress vertex_buffer;
  };

  struct Vertex {
    ran::Vec3f32 pos;
    f32 uv_x;
    ran::Vec3f32 normal;
    f32 uv_y;
    ran::Vec4f32 color;
  };

  struct ComputeRenderData {
    VkDescriptorSetLayout image_desc_layout;
    VkDescriptorSet image_desc;
    VkPipelineLayout layout;
    VkPipeline pipelines[2];
    ComputeConstants data[2];
    s32 effect_idx;
  };

  struct MeshRenderData {
    VkPipelineLayout triangle_layout;
    VkPipeline triangle_pipeline;
    VkPipelineLayout mesh_layout;
    VkPipeline mesh_pipeline;
    TypeBufferFor<VkAllocBuff> vertex_buffer;
    TypeBufferFor<VkAllocBuff> index_buffer;
    ran::Mat4f32 quad_transform;
  };

  struct MeshData {
    Span<const u32> indices;
    Span<const ran::Vec3f32> positions;
    Span<const ran::Vec3f32> normals;
    Span<const ran::Vec2f32> uvs;
    Span<const ran::Vec3f32> tangents;
    Span<const ran::Vec3f32> bitangents;
  };

  struct MeshAsset {
    ran::Mat4f32 transform;
    VkAllocBuff vertex_buffer;
    VkAllocBuff index_buffer;
    u32 index_start, index_count;
    std::string_view name;
  };

public:
  RenderContext(create_t, VkContext&& vk, GLFWContext::ImGuiHandler&& glfw_imgui,
                VkDelQueue&& delqueue, VkAllocImage&& target, VkDescAlloc&& desc_alloc,
                ComputeRenderData&& compute, MeshRenderData&& mesh);

public:
  static fn create(GLFWContext& glfw) -> VkMsgExpect<RenderContext>;
  fn destroy() -> void;

public:
  fn add_mesh(const MeshData& mesh, std::string_view name) -> void;

public:
  fn on_render(f64 dt, f64 alpha) -> void;
  fn on_fixed_update(u32 ups) -> void;

private:
  VkContext _vk;
  GLFWContext::ImGuiHandler _glfw_imgui;
  VkDelQueue _delqueue;
  VkAllocImage _target;
  VkDescAlloc _desc_alloc;
  ComputeRenderData _compute;
  MeshRenderData _mesh;
  Vec<MeshAsset> _meshes;
};

} // namespace kappa::render
