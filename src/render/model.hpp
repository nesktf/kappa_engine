#pragma once

#include "./common.hpp"

namespace kappa::render {

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
