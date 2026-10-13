#version 450
layout(set=0, binding=1) uniform sampler2D tex;
layout(location=0) out vec4 o;
void main() { o = texelFetch(tex, ivec2(gl_FragCoord.xy) % 64, 0); }
