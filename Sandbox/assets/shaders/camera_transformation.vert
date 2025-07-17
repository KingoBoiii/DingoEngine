#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout (std140, set = 0, binding = 0) uniform Camera {
	mat4 ProjectionView;
};

layout(location = 1) out vec3 fragColor;

void main() {
    gl_Position = ProjectionView * vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
}