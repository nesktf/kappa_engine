#pragma once

#include "../assets/model.hpp"
#include "./instance.hpp"

namespace kappa::render {

using pipeline_handle = u64;
using texture_handle = u64;

enum model_attrib_flag : bits32 {
  MODEL_ATTRIB_NONE = 0x0000,
  MODEL_ATTRIB_POSITIONS = 0x0001,
  MODEL_ATTRIB_NORMALS = 0x0002,
  MODEL_ATTRIB_TANGENTS = 0x0004,
  MODEL_ATTRIB_BONES = 0x0008,
  MODEL_ATTRIB_UV0 = 0x0010,
  MODEL_ATTRIB_UV1 = 0x0020,
  MODEL_ATTRIB_COLOR0 = 0x0040,
  MODEL_ATTRIB_COLOR1 = 0x0080,
};

struct model3d_mesh_data {
  size_t nverts;
  const v3f32* positions;
  const v3f32* normals;
  const v2f32* uvs;
  const v3f32* tangents;
  const v3f32* bitangents;
  const v4i32* bone_indices;
  const v4f32* bone_weights;
};

struct model3d_mesh {
  u32 prev, next;
  array_range textures;
  pipeline_handle pipeline;
};

struct mat4 {};

using v3 = float[3];
using v4 = float[4];
using v2 = float[2];
using i4 = int[4];
using bitfield = int;

struct light {
  v3 pos;
  v3 dir;
  v3 color;
  // ...
};

struct scene_things {
  const model* models;
  int model_count;

  light* lights;
  int light_count;

  mat4 proj, view;

  mat4* bone_matrices; // dynamic, depends on models
  int bone_matrices_count;
};

struct entity {
  int model;       // offset in assets::models
  int model_entry; // offsetin mesh::bone_matrices

  v3 pos;
  v3 scale;
  v4 rot_quat;

  float anim_weights[8];
  int anim_indices[8];
  int anim_count;
  float anim_time;
};

} // namespace kappa::render
