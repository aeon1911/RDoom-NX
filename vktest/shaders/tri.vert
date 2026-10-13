#version 450
layout(push_constant) uniform PC { float angle; float pad0; float pad1; float pad2; } pc;
layout(location = 0) out vec3 vColor;
void main()
{
	vec2 p[3] = vec2[](vec2(0.0, -0.6), vec2(0.55, 0.45), vec2(-0.55, 0.45));
	vec3 c[3] = vec3[](vec3(1.0, 0.25, 0.05), vec3(1.0, 0.65, 0.1), vec3(0.6, 0.05, 0.02));
	float s = sin(pc.angle), co = cos(pc.angle);
	vec2 v = p[gl_VertexIndex];
	v = vec2(v.x * co - v.y * s, v.x * s + v.y * co);
	v.x *= 720.0 / 1280.0;
	gl_Position = vec4(v, 0.0, 1.0);
	vColor = c[gl_VertexIndex];
}
