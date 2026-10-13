#version 450
layout(set=0, binding=1) uniform sampler2D tex;
layout(location=1) in vec2 vUV;
layout(location=0) out vec4 o;
void main() { o = texture(tex, vUV * 4.0); }
