#version 450
layout(push_constant) uniform PC { uint packed; } pc;
layout(location=0) out vec4 o;
void main() { o = unpackUnorm4x8(pc.packed); }   // byte extraction -> PRMT on NAK
