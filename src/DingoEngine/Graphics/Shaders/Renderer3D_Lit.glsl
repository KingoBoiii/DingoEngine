// Vertices arrive in world space (Renderer3D transforms them on the CPU while batching), so the
// vertex stage only applies the camera.

#type vertex
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec4 a_Color;

layout(std140, binding = 0) uniform CameraData
{
	mat4 ViewProjection;
	vec4 LightDirection; // xyz = direction the light travels
	vec4 Ambient;        // x = ambient strength
};

layout(location = 0) out vec3 v_Normal;
layout(location = 1) out vec4 v_Color;
layout(location = 2) out vec3 v_LightDir;
layout(location = 3) out float v_Ambient;

void main()
{
	gl_Position = ViewProjection * vec4(a_Position, 1.0);
	v_Normal    = a_Normal;
	v_Color     = a_Color;
	v_LightDir  = LightDirection.xyz;
	v_Ambient   = Ambient.x;
}

#type fragment
#version 450

layout(location = 0) in vec3 v_Normal;
layout(location = 1) in vec4 v_Color;
layout(location = 2) in vec3 v_LightDir;
layout(location = 3) in float v_Ambient;

// Material params (binding 1, following the scene UBO at binding 0). Only the built-in
// default material binds this — custom materials supply their own uniforms/layout.
layout(std140, binding = 1) uniform MaterialData
{
	vec4 EmissiveColor;    // rgb = color, a unused (padding)
	vec4 EmissiveStrength; // x = strength, yzw unused (padding)
};

layout(location = 0) out vec4 o_Color;

void main()
{
	vec3 normal = normalize(v_Normal);
	vec3 toLight = normalize(-v_LightDir);
	float diffuse = max(dot(normal, toLight), 0.0);
	float lighting = v_Ambient + (1.0 - v_Ambient) * diffuse;
	vec3 finalColor = v_Color.rgb * lighting;
	finalColor += EmissiveColor.rgb * EmissiveStrength.x;
	o_Color = vec4(finalColor, v_Color.a);
}
