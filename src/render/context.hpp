#pragma once

#include "./render/vulkan/vk_buffer.hpp"
#include "./render/vulkan/vk_context.hpp"
#include "./render/vulkan/vk_pipeline.hpp"
#include "./render/vulkan/vk_util.hpp"

#include "./glfw.hpp"

#include "../util/string.hpp"
#include "vulkan/vk_common.hpp"
#include "vulkan/vk_pipeline.hpp"

#include <ranmath/ran.hpp>

namespace kappa::render {

class RenderContext {
public:
  struct ComputeConstants {
    ran::Vec4f32 data1;
    ran::Vec4f32 data2;
    ran::Vec4f32 data3;
    ran::Vec4f32 data4;
  };

  struct ComputeRenderData {
    VkDescriptorSetLayout image_desc_layout;
    VkDescriptorSet image_desc;
    VkPipelineLayout layout;
    VkPipeline pipelines[2];
    ComputeConstants data[2];
    s32 effect_idx;
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
    VkPipeline pipeline;
    VkPipelineLayout layout;
    ran::Mat4f32 transform;
    VkAllocBuff vertex_buffer;
    VkAllocBuff index_buffer;
    u32 index_start, index_count;
    BuffStr<256> name;
  };

  struct DrawTarget {
    VkAllocImage color;
    VkAllocImage depth;
    VkExtent2D extent;
    f32 scale;
  };

  struct FrameData {
    VkDelQueue delqueue;
    VkDynDescAlloc desc_alloc;
  };

  using FrameArray = Vec<FrameData>; // We know the size but i can't be bothered right now

public:
  struct Self {
    VkContext vk;
    GLFWContext::ImGuiHandler glfw_imgui;
    VkDelQueue delqueue;
    VkDynDescAlloc desc_alloc;
    DrawTarget target;
    FrameArray frames;
    u32 frame_count;
    ComputeRenderData compute;
    Vec<MeshAsset> meshes;
    VkDescriptorSetLayout scene_layout;
  };

  KA_SELF_FORWARD(RenderContext);

public:
  static fn create(GLFWContext& glfw) -> VkMsgExpect<RenderContext>;
  fn destroy() -> void;

public:
  fn add_mesh(const MeshData& mesh, std::string_view name) -> void;

public:
  fn on_render(f64 dt, f64 alpha) -> void;
  fn on_fixed_update(u32 ups) -> void;
};

} // namespace kappa::render
