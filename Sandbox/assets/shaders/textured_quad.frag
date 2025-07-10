#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 vColor;
layout(location = 1) in vec2 vTexCoord;

layout(set = 0, binding = 0) uniform sampler2D uTextureSampler;

void main() {
    outColor = texture(uTextureSampler, vTexCoord);
}