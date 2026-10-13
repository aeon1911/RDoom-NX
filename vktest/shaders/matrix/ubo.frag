#version 450
layout(set=0, binding=0) uniform U { vec4 color; vec4 pad[3]; } u;
layout(location=0) out vec4 o;
void main() { o = u.color; }
