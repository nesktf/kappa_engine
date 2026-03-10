#include "./internal.hpp"

namespace kappa::render {

namespace {

constexpr char vert_src[] = R"glsl(
#version 440 core

layout (location = 0) in vec3 att_pos;
layout (location = 1) in vec3 att_norm;
layout (location = 2) in vec2 att_uvs;

layout (location = 0) out vec3 frag_norm;
layout (location = 1) out vec2 frag_uvs;

uniform mat4 u_proj;
uniform mat4 u_model;

void main() {
  gl_Position = u_proj*u_model*vec4(att_pos, 1.0f);
  frag_norm = att_norm;
  frag_uvs = att_uvs;
}  
)glsl";

constexpr char frag_src[] = R"glsl(
#version 440 core

layout (location = 0) in vec3 frag_norm;
layout (location = 1) in vec2 frag_uvs;

layout (location = 0) out vec4 out_color;

uniform sampler2D u_tex;
  
void main() {
  out_color = texture(u_tex, frag_uvs);
}
)glsl";

class pnttb_mesh_layout {
public:
  static constexpr size_t attribute_count = 7u;

public:
  pnttb_mesh_layout(size_t nverts) noexcept : _nverts(nverts) {}

public:
  shogle::vertex_attrib_array<attribute_count> attributes() const noexcept {
    const size_t pos_offset = 0;
    const size_t norm_offset = pos_offset;
    const size_t uv_offset = norm_offset + _nverts * sizeof(v3f32);
    const size_t tang_offset = uv_offset + _nverts * sizeof(v2f32);
    const size_t bitang_offset = tang_offset + _nverts * sizeof(v3f32);
    const size_t bonei_offset = bitang_offset + _nverts * sizeof(v3f32);
    const size_t bonew_offset = bonei_offset + _nverts * sizeof(v4i32);
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
       .type = shogle::attribute_type::vec2,
       .offset = uv_offset,
       .stride = sizeof(shogle::vec2)},
      {.location = 3,
       .type = shogle::attribute_type::vec3,
       .offset = tang_offset,
       .stride = sizeof(shogle::vec3)},
      {.location = 4,
       .type = shogle::attribute_type::vec3,
       .offset = bitang_offset,
       .stride = sizeof(shogle::vec3)},
      {.location = 5,
       .type = shogle::attribute_type::ivec4,
       .offset = bonei_offset,
       .stride = sizeof(shogle::ivec4)},
      {.location = 6,
       .type = shogle::attribute_type::vec4,
       .offset = bonew_offset,
       .stride = sizeof(shogle::vec2)},
    });
  }

private:
  size_t _nverts;
};

} // namespace

shogle::gl_s_expect<shogle::gl_pipeline> initialize_pipeline() {}

} // namespace kappa::render
