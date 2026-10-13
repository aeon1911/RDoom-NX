#version 450
layout(set=0, binding=4) uniform sampler2D tex3;
layout(location=1) in vec2 vUV;
layout(location=0) out vec4 o;
void main() { o = texture(tex3, vUV * 4.0); }
