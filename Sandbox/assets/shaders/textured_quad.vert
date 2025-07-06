#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(binding = 0) uniform sampler2D uTexture;

layout (std140, set = 0, binding = 256) uniform Camera {
	mat4 ProjectionView;
};

layout(location = 1) out vec4 fragColor;

void main() {
    gl_Position = ProjectionView * vec4(inPosition, 0.0, 1.0);
    fragColor = vec4(inTexCoord, 0.0, 1.0); // texture(uTexture, inTexCoord) * vec4(inColor, 1.0);

}