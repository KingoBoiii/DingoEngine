#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 vColor;
layout(location = 1) in vec2 vTexCoord;

layout(set = 0, binding = 0) uniform sampler2D uTexture;

void main() {
    // outColor = vec4(vTexCoord, 0.0, 1.0); 
    outColor = texture(uTexture, vTexCoord) * vec4(vColor, 1.0);
    //outColor = fragColor;
}