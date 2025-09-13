#pragma once

#include <string_view>

inline std::string_view vert_rigged_model_src = R"glsl(
#version 460 core

layout (location = 0) in vec3 att_positions;
layout (location = 1) in vec3 att_normals;
layout (location = 2) in vec2 att_uvs;
layout (location = 3) in vec3 att_tangents;
layout (location = 4) in vec3 att_bitangents;
layout (location = 5) in ivec4 att_bones;
layout (location = 6) in vec4 att_weights;

out VS_OUT {
  vec3 normals;
  vec2 uvs;
  vec3 tangents;
  vec3 bitangents;
} vs_out;

layout(std430, binding = 1) buffer bone_transforms {
  mat4 u_bone_mat[];
};

layout(std140, binding = 2) uniform scene_transforms {
  mat4 u_proj;
  mat4 u_view;
};

const int MAX_BONE_INFLUENCE = 4;

void main() {
  vec4 total_pos = vec4(0.f);
  for (int i = 0; i < MAX_BONE_INFLUENCE; ++i){
    if (att_bones[i] == -1) {
      continue;
    }
    vec4 local_pos = u_bone_mat[att_bones[i]] * vec4(att_positions, 1.f);
    total_pos += local_pos * att_weights[i];
    vec3 local_norm = mat3(u_bone_mat[att_bones[i]]) * att_normals;
  }

  gl_Position = u_proj*u_view*total_pos;

  vs_out.normals = att_normals;
  vs_out.uvs = att_uvs;
  vs_out.tangents = att_tangents;
  vs_out.bitangents = att_bitangents;
}
)glsl";

inline std::string_view vert_static_model_src = R"glsl(
#version 460 core

layout (location = 0) in vec3 att_positions;
layout (location = 1) in vec3 att_normals;
layout (location = 2) in vec2 att_uvs;
layout (location = 3) in vec3 att_tangents;
layout (location = 4) in vec3 att_bitangents;

out VS_OUT {
  vec3 normals;
  vec2 uvs;
  vec3 tangents;
  vec3 bitangents;
} vs_out;

layout(location = 1) uniform mat4 u_model;

layout(std140, binding = 2) uniform scene_transforms {
  mat4 u_proj;
  mat4 u_view;
};

void main() {
  gl_Position = u_proj*u_view*u_model*vec4(att_positions, 1.0f);

  vs_out.normals = att_normals;
  vs_out.uvs = att_uvs;
  vs_out.tangents = att_tangents;
  vs_out.bitangents = att_bitangents;
}

)glsl";

inline std::string_view vert_generic_model_src = R"glsl(
#version 460 core

layout (location = 0) in vec3 att_positions;
layout (location = 1) in vec3 att_normals;
layout (location = 2) in vec2 att_uvs;

out VS_OUT {
  vec3 normals;
  vec2 uvs;
} vs_out;

layout (location = 1) uniform mat4 u_model;

layout(std140, binding = 2) uniform scene_transforms {
  mat4 u_proj;
  mat4 u_view;
};

void main() {
  gl_Position = u_proj*u_view*u_model*vec4(att_positions, 1.0f);

  vs_out.normals = att_normals;
  vs_out.uvs = att_uvs;
}
)glsl";

inline std::string_view vert_skybox_src = R"glsl(
#version 460 core

layout (location = 0) in vec3 att_positions;

out VS_OUT {
  vec3 uvs;
} vs_out;

layout(std140, binding = 2) uniform scene_transforms {
  mat4 u_proj;
  mat4 u_view;
};

void main() {
  vec4 pos = u_proj*u_view*vec4(att_positions, 1.0f);
  gl_Position = vec4(pos.x, pos.y, pos.w, pos.w);

  vs_out.uvs = vec3(att_positions.x, att_positions.y, -att_positions.z);
}
)glsl";

inline std::string_view vert_sprite_src = R"glsl(
#version 460 core

layout (location = 0) in vec3 att_positions;
layout (location = 1) in vec3 att_normals;
layout (location = 2) in vec2 att_uvs;

out VS_OUT {
  vec3 normals;
  vec2 uvs;
} vs_out;

layout(std140, binding = 1) uniform sprite_transform {
  mat4 u_model;
  vec4 u_offset;
};

layout(std140, binding = 2) uniform scene_transforms {
  mat4 u_view;
  mat4 u_proj;
};

void main() {
  gl_Position = u_proj*u_view*u_model*vec4(att_positions, 1.0f);

  vs_out.normals = att_normals;
  vs_out.uvs.x = att_uvs.x*u_offset.x + u_offset.z;
  vs_out.uvs.y = att_uvs.y*u_offset.y + u_offset.w;
}
)glsl";

inline std::string_view vert_effect_src = R"glsl(
#version 460 core

layout (location = 0) in vec3 att_positions;
layout (location = 1) in vec3 att_normals;
layout (location = 2) in vec2 att_uvs;

out VS_OUT {
  vec3 normals;
  vec2 uvs;
} vs_out;

void main() {
  gl_Position = vec4(att_positions.x*2.f, att_positions.y*2.f, att_positions.z*2.f, 1.f);

  vs_out.normals = att_normals;
  vs_out.uvs = att_uvs;
}
)glsl";

inline std::string_view frag_header_base_src = R"glsl(
#version 460 core

out vec4 frag_color;
)glsl";

inline std::string_view frag_tangents_base_src = R"glsl(
in VS_OUT {
  vec3 normals;
  vec2 uvs;
  vec3 tangents;
  vec3 bitangents;
} fs_in;
)glsl";

inline std::string_view frag_normals_base_src = R"glsl(
in VS_OUT {
  vec3 normals;
  vec2 uvs;
} fs_in;
)glsl";

inline std::string_view frag_skybox_base_src = R"glsl(
in VS_OUT {
  vec3 uvs;
} fs_in;
)glsl";

inline std::string_view frag_raw_albedo_src = R"glsl(
layout(location = 8) uniform sampler2D u_albedo;

void main() {
  frag_color = texture(u_albedo, fs_in.uvs);
}
)glsl";
