#version 460

layout (location = 0) out vec3 out_color;
layout (location = 1) out vec2 out_uv;

const vec3 positions[3] = vec3[3](
  vec3( 1.f,  1.f,  0.f),
	vec3(-1.f,  1.f,  0.f),
	vec3( 0.f, -1.f,  0.f)
);

const vec3 colors[3] = vec3[3](
  vec3( 1.f,  0.f,  0.f),
	vec3( 0.f,  1.f,  0.f),
	vec3( 0.f,  0.f,  1.f)
);
const vec2 uvs[3] = vec2[3](
  vec2(1.f, 1.f),
  vec2(0.f, 1.f),
  vec2(.5f, 0.f)
);

void main() {
  gl_Position = vec4(positions[gl_VertexIndex], 1.f);
	out_color = colors[gl_VertexIndex];
	out_uv.x = uvs[gl_VertexIndex].x;
	out_uv.y = uvs[gl_VertexIndex].y;
}