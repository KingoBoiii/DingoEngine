#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout (std140, set = 0, binding = 256) uniform Camera {
	mat4 ProjectionView;
};

layout(location = 0) out vec3 vColor;
layout(location = 1) out vec2 vTexCoord;

void main() {
    gl_Position = ProjectionView * vec4(inPosition, 0.0, 1.0);
    vColor = inColor;
    vTexCoord = inTexCoord;
}