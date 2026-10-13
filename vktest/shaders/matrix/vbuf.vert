#version 450
layout(location=0) in vec2 inPos;
layout(location=1) in vec4 inColor;      // R8G8B8A8_UNORM vertex attribute
layout(location=0) out vec4 vColor;
layout(location=1) out vec2 vUV;
void main() { gl_Position = vec4(inPos, 0.0, 1.0); vColor = inColor; vUV = inPos * 0.5 + 0.5; }
