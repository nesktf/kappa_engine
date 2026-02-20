#include "./instance.hpp"

namespace kappa::render {

namespace {

constexpr std::string_view vert_rigged_model_src = R"glsl(
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

constexpr std::string_view vert_static_model_src = R"glsl(
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

constexpr std::string_view vert_generic_model_src = R"glsl(
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

constexpr std::string_view vert_skybox_src = R"glsl(
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

constexpr std::string_view vert_sprite_src = R"glsl(
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

constexpr std::string_view vert_effect_src = R"glsl(
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

constexpr std::string_view frag_header_base_src = R"glsl(
#version 460 core

out vec4 frag_color;
)glsl";

constexpr std::string_view frag_tangents_base_src = R"glsl(
in VS_OUT {
  vec3 normals;
  vec2 uvs;
  vec3 tangents;
  vec3 bitangents;
} fs_in;
)glsl";

// constexpr std::string_view frag_normals_base_src = R"glsl(
// in VS_OUT {
//   vec3 normals;
//   vec2 uvs;
// } fs_in;
// )glsl";
//
// constexpr std::string_view frag_skybox_base_src = R"glsl(
// in VS_OUT {
//   vec3 uvs;
// } fs_in;
// )glsl";

constexpr std::string_view frag_raw_albedo_src = R"glsl(
layout(location = 8) uniform sampler2D u_albedo;

void main() {
  frag_color = texture(u_albedo, fs_in.uvs);
}
)glsl";

} // namespace

vert_shader_array initialize_shaders(shogle::context_view ctx) {
  auto vert_rigged = shogle::vertex_shader::create(ctx, {vert_rigged_model_src}).value();
  auto vert_static = shogle::vertex_shader::create(ctx, {vert_static_model_src}).value();
  auto vert_generic = shogle::vertex_shader::create(ctx, {vert_generic_model_src}).value();
  auto vert_skybox = shogle::vertex_shader::create(ctx, {vert_skybox_src}).value();
  auto vert_sprite = shogle::vertex_shader::create(ctx, {vert_sprite_src}).value();
  auto vert_effect = shogle::vertex_shader::create(ctx, {vert_effect_src}).value();
  return {std::move(vert_rigged), std::move(vert_static), std::move(vert_generic),
          std::move(vert_skybox), std::move(vert_sprite), std::move(vert_effect)};
}

shogle::vertex_shader_view make_vert_stage(vert_shader_type type, u32& flags,
                                           std::vector<shogle::attribute_binding>& bindings,
                                           vert_shader_array& verts, bool aos_bindings) {
  struct rig_vertex {
    vec3 pos;
    vec3 norm;
    vec2 uvs;
    vec3 tang;
    vec3 bitang;
    ivec4 bones;
    vec4 weights;
  };

  struct tang_vertex {
    vec3 pos;
    vec3 norm;
    vec2 uvs;
    vec3 tang;
    vec3 bitang;
  };

  struct generic_vertex {
    vec3 pos;
    vec3 norm;
    vec2 uvs;
  };

  struct skybox_vertex {
    vec3 pos;
  };

  switch (type) {
    case VERT_SHADER_RIGGED_MODEL: {
      bindings.reserve(7u);
      // type, location, offset, stride
      bindings.emplace_back(shogle::attribute_type::vec3, 0u,
                            aos_bindings * offsetof(rig_vertex, pos),
                            aos_bindings * sizeof(rig_vertex));
      bindings.emplace_back(shogle::attribute_type::vec3, 1u,
                            aos_bindings * offsetof(rig_vertex, norm),
                            aos_bindings * sizeof(rig_vertex));
      bindings.emplace_back(shogle::attribute_type::vec2, 2u,
                            aos_bindings * offsetof(rig_vertex, uvs),
                            aos_bindings * sizeof(rig_vertex));
      bindings.emplace_back(shogle::attribute_type::vec3, 3u,
                            aos_bindings * offsetof(rig_vertex, tang),
                            aos_bindings * sizeof(rig_vertex));
      bindings.emplace_back(shogle::attribute_type::vec3, 4u,
                            aos_bindings * offsetof(rig_vertex, bitang),
                            aos_bindings * sizeof(rig_vertex));
      bindings.emplace_back(shogle::attribute_type::ivec4, 5u,
                            aos_bindings * offsetof(rig_vertex, bones),
                            aos_bindings * sizeof(rig_vertex));
      bindings.emplace_back(shogle::attribute_type::vec4, 6u,
                            aos_bindings * offsetof(rig_vertex, weights),
                            aos_bindings * sizeof(rig_vertex));
      flags = VERTEX_STAGE_EXPORTS_NORMALS | VERTEX_STAGE_EXPORTS_TANGENTS |
              VERTEX_STAGE_SCENE_TRANSFORMS | VERTEX_STAGE_MODEL_ARRAY;
      break;
    }
    case VERT_SHADER_STATIC_MODEL: {
      bindings.reserve(5u);
      // type, location, offset, stride
      bindings.emplace_back(shogle::attribute_type::vec3, 0u,
                            aos_bindings * offsetof(tang_vertex, pos),
                            aos_bindings * sizeof(tang_vertex));
      bindings.emplace_back(shogle::attribute_type::vec3, 1u,
                            aos_bindings * offsetof(tang_vertex, norm),
                            aos_bindings * sizeof(tang_vertex));
      bindings.emplace_back(shogle::attribute_type::vec2, 2u,
                            aos_bindings * offsetof(tang_vertex, uvs),
                            aos_bindings * sizeof(tang_vertex));
      bindings.emplace_back(shogle::attribute_type::vec3, 3u,
                            aos_bindings * offsetof(tang_vertex, tang),
                            aos_bindings * sizeof(tang_vertex));
      bindings.emplace_back(shogle::attribute_type::vec3, 4u,
                            aos_bindings * offsetof(tang_vertex, bitang),
                            aos_bindings * sizeof(tang_vertex));
      flags = VERTEX_STAGE_EXPORTS_NORMALS | VERTEX_STAGE_EXPORTS_TANGENTS |
              VERTEX_STAGE_SCENE_TRANSFORMS | VERTEX_STAGE_MODEL_MATRIX;
      break;
    }
    case VERT_SHADER_GENERIC_MODEL: {
      bindings.reserve(3u);
      // type, location, offset, stride
      bindings.emplace_back(shogle::attribute_type::vec3, 0u,
                            aos_bindings * offsetof(generic_vertex, pos),
                            aos_bindings * sizeof(generic_vertex));
      bindings.emplace_back(shogle::attribute_type::vec3, 1u,
                            aos_bindings * offsetof(generic_vertex, norm),
                            aos_bindings * sizeof(generic_vertex));
      bindings.emplace_back(shogle::attribute_type::vec2, 2u,
                            aos_bindings * offsetof(generic_vertex, uvs),
                            aos_bindings * sizeof(generic_vertex));
      flags =
        VERTEX_STAGE_EXPORTS_NORMALS | VERTEX_STAGE_SCENE_TRANSFORMS | VERTEX_STAGE_MODEL_MATRIX;
      break;
    }
    case VERT_SHADER_SKYBOX: {
      bindings.reserve(1u);
      bindings.emplace_back(shogle::attribute_type::vec3, 0u,
                            aos_bindings * offsetof(skybox_vertex, pos),
                            aos_bindings * sizeof(skybox_vertex));
      flags = VERTEX_STAGE_SCENE_TRANSFORMS | VERTEX_STAGE_MODEL_NONE;
      break;
    }
    case VERT_SHADER_SPRITE: {
      bindings.reserve(3u);
      // type, location, offset, stride
      bindings.emplace_back(shogle::attribute_type::vec3, 0u,
                            aos_bindings * offsetof(generic_vertex, pos),
                            aos_bindings * sizeof(generic_vertex));
      bindings.emplace_back(shogle::attribute_type::vec3, 1u,
                            aos_bindings * offsetof(generic_vertex, norm),
                            aos_bindings * sizeof(generic_vertex));
      bindings.emplace_back(shogle::attribute_type::vec2, 2u,
                            aos_bindings * offsetof(generic_vertex, uvs),
                            aos_bindings * sizeof(generic_vertex));
      flags =
        VERTEX_STAGE_EXPORTS_NORMALS | VERTEX_STAGE_SCENE_TRANSFORMS | VERTEX_STAGE_MODEL_OFFSET;
      break;
    }
    case VERT_SHADER_EFFECT: {
      bindings.reserve(3u);
      // type, location, offset, stride
      bindings.emplace_back(shogle::attribute_type::vec3, 0u,
                            aos_bindings * offsetof(generic_vertex, pos),
                            aos_bindings * sizeof(generic_vertex));
      bindings.emplace_back(shogle::attribute_type::vec3, 1u,
                            aos_bindings * offsetof(generic_vertex, norm),
                            aos_bindings * sizeof(generic_vertex));
      bindings.emplace_back(shogle::attribute_type::vec2, 2u,
                            aos_bindings * offsetof(generic_vertex, uvs),
                            aos_bindings * sizeof(generic_vertex));
      flags = VERTEX_STAGE_EXPORTS_NORMALS;
      break;
    }
    default:
      NTF_UNREACHABLE();
  }
  return verts[type];
}

expect<shogle::fragment_shader> make_frag_stage(shogle::context_view ctx, frag_shader_type type,
                                                u32 vert_flags) {
  NTF_UNUSED(type);
  NTF_UNUSED(vert_flags);
  std::vector<shogle::string_view> srcs;
  srcs.emplace_back(frag_header_base_src);
  srcs.emplace_back(frag_tangents_base_src);
  srcs.emplace_back(frag_raw_albedo_src);

  return shogle::fragment_shader::create(ctx, {srcs.data(), srcs.size()})
    .transform_error([](shogle::render_error&& err) -> std::string {
      std::string base = err.what();
      if (err.has_msg()) {
        base.append(err.msg().data(), err.msg().size());
      }
      return base;
    });
}

} // namespace kappa::render
