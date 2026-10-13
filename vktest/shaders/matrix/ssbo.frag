#version 450
layout(set=0, binding=2, std430) readonly buffer S { vec4 colors[4]; } s;
layout(location=0) out vec4 o;
void main() { o = s.colors[int(gl_FragCoord.x) / 64 % 4]; }
