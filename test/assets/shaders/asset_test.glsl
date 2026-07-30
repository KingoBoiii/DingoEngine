#type vertex
#version 450

layout(location = 0) in vec2 inPosition;

layout (std140, set = 0, binding = 0) uniform Camera {
	mat4 ProjectionView;
};

void main() {
	gl_Position = ProjectionView * vec4(inPosition, 0.0, 1.0);
}

#type fragment
#version 450

layout(location = 0) out vec4 outColor;

void main() {
	outColor = vec4(0.9, 0.4, 0.1, 1.0);
}
