#include "./internal.hpp"

namespace kappa::render {

namespace {

constexpr char glsl_common_header[] = R"glsl(
#version 460 core

const uint SAMPLER_ALBEDO = 0;
const uint SAMPLER_SPECULAR = 1;
const uint SAMPLER_NORMAL = 2;
const uint SAMPLER_AMBIENT_OCCLUSION = 3;
const uint SAMPLER_ROUGHNESS = 4;
const uint SAMPLER_METALLIC = 5;

struct scene_data {
  mat4 proj;
  mat4 view;
};
struct instance_data {
  mat4 model;
};
layout(std430, binding = 1) buffer scene_buffer {
  scene_data u_scene;
};
layout(std430, binding = 2) buffer instance_buffer {
  instance_data u_instances[];
};
)glsl";

constexpr u32 glsl_att_pos = 0;
constexpr u32 glsl_att_norm = 1;
constexpr u32 glsl_att_uv = 2;
constexpr u32 glsl_att_tang = 3;
constexpr u32 glsl_att_bitang = 4;
constexpr char glsl_vert_tangent_header[] = R"glsl(
layout (location = 0) in vec3 att_pos;
layout (location = 1) in vec3 att_norm;
layout (location = 2) in vec2 att_uvs;
layout (location = 3) in vec3 att_tang;
layout (location = 4) in vec3 att_bitang;

out VS_OUT {
  vec3 vert_normals;
  vec3 vert_tangents;
  vec3 vert_bitangents;
  vec2 vert_uvs;
} vs_out;
)glsl";
constexpr char glsl_vert_tangent_body[] = R"glsl(
void main() {
  gl_Position = u_scene.proj * u_scene.view * u_instances[gl_InstanceID].model * model_pos();

  vs_out.vert_normals = att_norm;
  vs_out.vert_tangents = att_tang;
  vs_out.vert_bitangents = att_bitang;
  vs_out.vert_uvs = att_uvs;
}
)glsl";
constexpr char glsl_vert_notangent_header[] = R"glsl(
layout (location = 0) in vec3 att_pos;
layout (location = 1) in vec3 att_norm;
layout (location = 2) in vec2 att_uvs;

out VS_OUT {
  vec3 vert_normals;
  vec2 vert_uvs;
} vs_out;
)glsl";
constexpr char glsl_vert_notangent_body[] = R"glsl(
void main() {
  gl_Position = u_scene.proj * u_scene.view * u_instances[gl_InstanceID].model * model_pos();

  vs_out.vert_normals = att_norm;
  vs_out.vert_uvs = att_uvs;
}
)glsl";

constexpr u32 glsl_att_bone_indices = 5;
constexpr u32 glsl_att_bone_weights = 6;
constexpr char glsl_vert_bone_header[] = R"glsl(
layout (location = 5) in ivec4 att_bone_indices;
layout (location = 6) in vec4 att_bone_weights;

const int MAX_BONE_INFLUENCE = 4;

vec4 model_pos() {
  vec4 total_pos = vec4(0.f);
  for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
    if (att_bone_indices[i] == -1) {
      continue;
    }
    vec4 local_pos = u_bone_mat[att_bone_indices[i]] * vec4(att_pos, 1.f);
    total_pos += local_pos * att_bone_weights[i];
    //vec3 local_norm = mat3(u_bone_mat[att_bone_indices[i]]) * att_normals;
  }
  return total_pos;
}
)glsl";
constexpr char glsl_vert_nobone_header[] = R"glsl(
vec4 model_pos() {
  return vec4(att_pos, 1.f);
}
)glsl";

constexpr char glsl_frag_tangent_header[] = R"glsl(
layout(location = 1) uniform sampler2D u_samplers[6];

in VS_OUT {
  vec3 vert_normals;
  vec3 vert_tangents;
  vec3 vert_bitangents;
  vec2 vert_uvs;
} fs_in;
)glsl";
constexpr char glsl_frag_notangent_header[] = R"glsl(
layout(location = 1) uniform sampler2D u_samplers[6];

in VS_OUT {
  vec3 vert_normals;
  vec2 vert_uvs;
} fs_in;
)glsl";

constexpr char glsl_frag_body[] = R"glsl(
void main() {
  frag_color = texture(u_samplers[SAMPLER_ALBEDO], fs_in.vert_uvs);
}
)glsl";

class pnt_mesh_layout {
public:
  static constexpr size_t attribute_count = 3u;

public:
  pnt_mesh_layout(size_t offset, size_t nverts) noexcept : _offset(offset), _nverts(nverts) {}

public:
  static bool test_mesh(bits32 attrib) noexcept {
    return (attrib & ATTRIB_FLAG_POSITIONS) && (attrib & ATTRIB_FLAG_NORMALS) &&
           (attrib & ATTRIB_FLAG_UV0);
  }

public:
  shogle::vertex_attrib_array<attribute_count> attributes() const noexcept {
    const size_t pos_offset = _offset;
    const size_t norm_offset = pos_offset + _nverts * sizeof(v3f32);
    const size_t uv_offset = norm_offset + _nverts * sizeof(v3f32);
    return std::to_array<shogle::vertex_attribute>({
      {.location = glsl_att_pos,
       .type = shogle::attribute_type::vec3,
       .offset = pos_offset,
       .stride = sizeof(v3f32)},
      {.location = glsl_att_norm,
       .type = shogle::attribute_type::vec3,
       .offset = norm_offset,
       .stride = sizeof(v3f32)},
      {.location = glsl_att_uv,
       .type = shogle::attribute_type::vec2,
       .offset = uv_offset,
       .stride = sizeof(v2f32)},
    });
  }

private:
  size_t _offset, _nverts;
};

class pnttb_mesh_layout {
public:
  static constexpr size_t attribute_count = 7u;

public:
  pnttb_mesh_layout(size_t offset, size_t nverts) noexcept : _offset(offset), _nverts(nverts) {}

public:
  static bool test_mesh(bits32 attrib) noexcept {
    return (attrib & ATTRIB_FLAG_POSITIONS) && (attrib & ATTRIB_FLAG_NORMALS) &&
           (attrib & ATTRIB_FLAG_TANGENTS) && (attrib & ATTRIB_FLAG_BONES) &&
           (attrib & ATTRIB_FLAG_UV0);
  }

public:
  shogle::vertex_attrib_array<attribute_count> attributes() const noexcept {
    const size_t pos_offset = _offset;
    const size_t norm_offset = pos_offset + _nverts * sizeof(v3f32);
    const size_t uv_offset = norm_offset + _nverts * sizeof(v3f32);
    const size_t tang_offset = uv_offset + _nverts * sizeof(v2f32);
    const size_t bitang_offset = tang_offset + _nverts * sizeof(v3f32);
    const size_t bone_indices_offset = bitang_offset + _nverts * sizeof(v3f32);
    const size_t bone_weights_offset = bone_indices_offset + _nverts * sizeof(v4i32);
    return std::to_array<shogle::vertex_attribute>({
      {.location = glsl_att_pos,
       .type = shogle::attribute_type::vec3,
       .offset = pos_offset,
       .stride = sizeof(v3f32)},
      {.location = glsl_att_norm,
       .type = shogle::attribute_type::vec3,
       .offset = norm_offset,
       .stride = sizeof(v3f32)},
      {.location = glsl_att_uv,
       .type = shogle::attribute_type::vec2,
       .offset = uv_offset,
       .stride = sizeof(v2f32)},
      {.location = glsl_att_tang,
       .type = shogle::attribute_type::vec3,
       .offset = tang_offset,
       .stride = sizeof(v3f32)},
      {.location = glsl_att_bitang,
       .type = shogle::attribute_type::vec3,
       .offset = bitang_offset,
       .stride = sizeof(v3f32)},
      {.location = glsl_att_bone_indices,
       .type = shogle::attribute_type::ivec4,
       .offset = bone_indices_offset,
       .stride = sizeof(v4i32)},
      {.location = glsl_att_bone_weights,
       .type = shogle::attribute_type::vec4,
       .offset = bone_weights_offset,
       .stride = sizeof(v4f32)},
    });
  }

private:
  size_t _offset, _nverts;
};

struct shader_pair {
  shogle::gl_shader vert, frag;
};

fn generate_shaders(bits32 attribs) -> shogle::gl_s_expect<shader_pair> {
  assert(g_ctx.has_value());
  std::string vert_src = glsl_common_header;
  std::string frag_src = glsl_common_header;
  if (pnttb_mesh_layout::test_mesh(attribs)) {
    vert_src += glsl_vert_tangent_header;
    vert_src += glsl_vert_bone_header;
    vert_src += glsl_vert_tangent_body;

    frag_src += glsl_frag_tangent_header;
    frag_src += glsl_frag_body;
  } else if (pnt_mesh_layout::test_mesh(attribs)) {
    vert_src += glsl_vert_notangent_header;
    vert_src += glsl_vert_nobone_header;
    vert_src += glsl_vert_notangent_body;

    frag_src += glsl_frag_notangent_header;
    frag_src += glsl_frag_body;
  } else {
    return {unexpect, "Invalid mesh layout"};
  }

  auto vert = shogle::gl_shader::create(g_ctx->gl, vert_src.data(), vert_src.size(),
                                        shogle::gl_shader::STAGE_VERTEX);
  if (!vert) {
    return {unexpect, std::move(vert).error()};
  }
  auto frag = shogle::gl_shader::create(g_ctx->gl, frag_src.data(), frag_src.size(),
                                        shogle::gl_shader::STAGE_FRAGMENT);
  if (!frag) {
    shogle::gl_shader::destroy(g_ctx->gl, *vert);
    return {unexpect, std::move(frag).error()};
  }
  return {in_place, *std::move(vert), *std::move(frag)};
}

fn make_vertex_layout(buffer_handle attrib_buff, size_t nverts, size_t vertex_offset,
                      size_t index_offset, bits32 attribs)
  -> shogle::gl_expect<shogle::gl_vertex_layout> {
  shogle::gl_layout_builder layout_builder;
  auto& buff = g_ctx->buffers[(u32)attrib_buff];
  layout_builder.set_vertex_buffer(buff);
  if (index_offset) {
    layout_builder.set_index_buffer(buff, shogle::gl_vertex_layout::INDEX_FORMAT_U32)
      .set_index_offset(index_offset);
  }

  if (pnttb_mesh_layout::test_mesh(attribs)) {
    pnttb_mesh_layout layout(vertex_offset, nverts);
    return layout_builder.build(g_ctx->gl, layout);
  } else if (pnt_mesh_layout::test_mesh(attribs)) {
    pnt_mesh_layout layout(vertex_offset, nverts);
    return layout_builder.build(g_ctx->gl, layout);
  }
  return {unexpect, 0};
}

} // namespace

s_expect<pipeline_handle> create_pipeline(const pipeline_create_data& data) {
  assert(g_ctx.has_value());
  assert(!is_nil_handle(data.buffer));

  auto shad = generate_shaders(data.vertex_attributes);
  if (!shad) {
    return {unexpect, std::move(shad).error().what()};
  }
  const shogle::scope_end shad_defer = [&]() {
    shogle::gl_shader::destroy(g_ctx->gl, shad->vert);
    shogle::gl_shader::destroy(g_ctx->gl, shad->frag);
  };

  auto vert_layout = make_vertex_layout(data.buffer, data.nverts, data.vertex_offset,
                                        data.index_offset, data.vertex_attributes);
  if (!vert_layout) {
    return {unexpect, vert_layout.error().what()};
  }

  shogle::gl_pipeline_builder pip_builder;
  auto pip = pip_builder.set_primitive(shogle::gl_pipeline::PRIMITIVE_TRIANGLES)
               .set_polygon_mode(shogle::gl_pipeline::POLY_MODE_FILL)
               .set_polygon_width(1.f)
               .add_shader(shad->vert)
               .add_shader(shad->frag)
               .set_depth_test(shogle::gl_depth_test_props::make_default(true))
               .build(g_ctx->gl);

  if (!pip) {
    shogle::gl_vertex_layout::destroy(g_ctx->gl, *vert_layout);
    return {unexpect, pip.error().what()};
  }

  auto slot = g_ctx->pipelines.emplace(std::move(*pip), std::move(*vert_layout));
  return {in_place, (pipeline_handle)slot};
}

void destroy_pipeline(pipeline_handle pipeline) {
  assert(g_ctx.has_value());
  if (auto* pip = g_ctx->pipelines.at_opt((u32)pipeline)) {
    shogle::gl_pipeline::destroy(g_ctx->gl, pip->pipeline);
    shogle::gl_vertex_layout::destroy(g_ctx->gl, pip->layout);
    g_ctx->pipelines.remove((u32)pipeline);
  }
}

} // namespace kappa::render
