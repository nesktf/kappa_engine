#include "./internal.hpp"

namespace kappa::render {

namespace {

constexpr char vert_src2[] = R"glsl(
#version 460 core

layout (location = 0) in vec3 att_pos;
layout (location = 1) in vec3 att_norm;
layout (location = 2) in vec3 att_tang;
layout (location = 3) in vec3 att_bitang;
layout (location = 4) in ivec4 att_bone_indices;
layout (location = 5) in vec4 att_bone_weights;
layout (location = 6) in vec2 att_uvs;

layout (location = 0) out vec3 frag_norm;

layout (location = 1) uniform mat4 u_proj;
layout (location = 2) uniform  mat4 u_model;

void main() {
  gl_Position = u_proj*u_model*vec4(att_pos, 1.0f);
}  
)glsl";

constexpr char vert_src[] = R"glsl(
#version 460 core

layout (location = 0) in vec3 att_pos;
layout (location = 1) in vec3 att_norm;

layout (location = 0) out vec3 frag_norm;

layout (location = 1) uniform mat4 u_proj;
layout (location = 2) uniform  mat4 u_model;

void main() {
  gl_Position = u_proj*u_model*vec4(att_pos, 1.0f);
  frag_norm = att_norm;
}  
)glsl";

constexpr char frag_src[] = R"glsl(
#version 460 core

//layout (location = 0) in vec3 frag_norm;
//layout (location = 1) in vec2 frag_uvs;

layout (location = 0) out vec4 out_color;

//uniform sampler2D u_tex;
  
void main() {
out_color = vec4(1.0, 0.0, 0.0, 1.0);
//  out_color = texture(u_tex, frag_uvs);
}
)glsl";

struct shader_thing {
  shogle::gl_shader vert, frag;
};

class pn_mesh_layout {
public:
  static constexpr size_t attribute_count = 2u;

public:
  pn_mesh_layout(size_t offset, size_t nverts) noexcept : _offset(offset), _nverts(nverts) {}

public:
  static bool test_mesh(bits32 attrib) noexcept {
    return (attrib & ATTRIB_FLAG_POSITIONS) && (attrib & ATTRIB_FLAG_NORMALS);
  }

public:
  shogle::vertex_attrib_array<attribute_count> attributes() const noexcept {
    const size_t pos_offset = _offset;
    const size_t norm_offset = pos_offset + _nverts * sizeof(v3f32);
    return std::to_array<shogle::vertex_attribute>({
      {.location = 0,
       .type = shogle::attribute_type::vec3,
       .offset = pos_offset,
       .stride = sizeof(shogle::vec3)},
      {.location = 1,
       .type = shogle::attribute_type::vec3,
       .offset = norm_offset,
       .stride = sizeof(shogle::vec3)},
    });
  }

private:
  size_t _offset;
  size_t _nverts;
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
    const size_t tang_offset = norm_offset + _nverts * sizeof(v3f32);
    const size_t bitang_offset = tang_offset + _nverts * sizeof(v3f32);
    const size_t bonei_offset = bitang_offset + _nverts * sizeof(v3f32);
    const size_t bonew_offset = bonei_offset + _nverts * sizeof(v4i32);
    const size_t uv_offset = bonew_offset + _nverts * sizeof(v4f32);
    return std::to_array<shogle::vertex_attribute>({
      {.location = 0,
       .type = shogle::attribute_type::vec3,
       .offset = pos_offset,
       .stride = sizeof(shogle::vec3)},
      {.location = 1,
       .type = shogle::attribute_type::vec3,
       .offset = norm_offset,
       .stride = sizeof(shogle::vec3)},
      {.location = 2,
       .type = shogle::attribute_type::vec3,
       .offset = tang_offset,
       .stride = sizeof(shogle::vec3)},
      {.location = 3,
       .type = shogle::attribute_type::vec3,
       .offset = bitang_offset,
       .stride = sizeof(shogle::vec3)},
      {.location = 4,
       .type = shogle::attribute_type::ivec4,
       .offset = bonei_offset,
       .stride = sizeof(shogle::ivec4)},
      {.location = 5,
       .type = shogle::attribute_type::vec4,
       .offset = bonew_offset,
       .stride = sizeof(shogle::vec4)},
      {.location = 6,
       .type = shogle::attribute_type::vec2,
       .offset = uv_offset,
       .stride = sizeof(shogle::vec2)},
    });
  }

private:
  size_t _offset;
  size_t _nverts;
};

fn generate_shaders(bits32 attribs, bits32 texes) -> s_expect<shader_thing> {
  (void)texes;
  assert(g_ctx.has_value());
  if (pnttb_mesh_layout::test_mesh(attribs)) {
    shogle::gl_shader vert(g_ctx->gl, vert_src2, sizeof(vert_src2),
                           shogle::gl_shader::STAGE_VERTEX);
    shogle::gl_shader frag(g_ctx->gl, frag_src, sizeof(frag_src),
                           shogle::gl_shader::STAGE_FRAGMENT);
    return {in_place, vert, frag};
  } else {
    shogle::gl_shader vert(g_ctx->gl, vert_src, sizeof(vert_src), shogle::gl_shader::STAGE_VERTEX);
    shogle::gl_shader frag(g_ctx->gl, frag_src, sizeof(frag_src),
                           shogle::gl_shader::STAGE_FRAGMENT);
    return {in_place, vert, frag};
  }
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
  } else if (pn_mesh_layout::test_mesh(attribs)) {
    pn_mesh_layout layout(vertex_offset, nverts);
    return layout_builder.build(g_ctx->gl, layout);
  }
  return {unexpect, 0};
}

} // namespace

s_expect<pipeline_handle> create_pipeline(const pipeline_create_data& data) {
  assert(g_ctx.has_value());
  assert(!is_nil_handle(data.buffer));

  auto shad = generate_shaders(data.vertex_attributes, data.material_textures);
  if (!shad) {
    return {unexpect, std::move(shad).error()};
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
