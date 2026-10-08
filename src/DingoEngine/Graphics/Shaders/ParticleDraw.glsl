// Renderer3D's particle draw: one instance a pool slot of a batch's emitters (those sharing a blend and
// a sprite), a camera-facing quad of six vertices, nothing for a dead slot. Unlit. Mirrors
// Internal::ParticleDrawData in ParticleRenderer.cpp.

#type vertex
#version 450

#define DE_PARTICLE_EMITTERS_BINDING 6
#include <DingoEngine/ParticleCommon.glsl>

layout(std140, binding = 0) uniform ParticleDraw
{
	mat4 ViewProjection;
	mat4 InverseViewProjection;
	vec4 CameraRight;
	vec4 CameraUp;
	uvec4 Batch;  // x = first emitter, y = emitters, z = their first thread, w = flags: 1 soft, 2 round dot, 4 additive
	vec4 Target;  // xy = one target pixel in UV
};

layout(std430, binding = 5) readonly buffer ParticlePool
{
	Particle Particles[];
};

layout(location = 0) out vec4 v_Color;
layout(location = 1) out vec2 v_TexCoord;
layout(location = 2) out vec2 v_Corner;
layout(location = 3) out float v_SoftDistance;

const vec2 CORNERS[6] = vec2[](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0));

void main()
{
	uint thread = Batch.z + uint(gl_InstanceIndex);
	ParticleEmitter emitter = Emitters[FindEmitter(thread, Batch.x, Batch.y)];
	Particle particle = Particles[emitter.Ring.x + (thread - emitter.Ring.z)];

	vec2 corner = CORNERS[gl_VertexIndex];
	v_Corner = corner;
	v_SoftDistance = emitter.Size.w;
	if (particle.VelocityLife.w <= 0.0)
	{
		gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
		v_Color = vec4(0.0);
		v_TexCoord = vec2(0.0);
		return;
	}

	float t = clamp(particle.PositionAge.w / particle.VelocityLife.w, 0.0, 1.0);
	vec3 center = particle.PositionAge.xyz;
	if ((emitter.Ring.w & PARTICLE_WORLD_SPACE) == 0u)
		center = (emitter.Transform * vec4(center, 1.0)).xyz;

	float size = particle.Misc.x * mix(1.0, emitter.Size.z, t);
	float c = cos(particle.Misc.y);
	float s = sin(particle.Misc.y);
	vec2 turned = vec2(corner.x * c - corner.y * s, corner.x * s + corner.y * c);
	vec3 world = center + (CameraRight.xyz * turned.x + CameraUp.xyz * turned.y) * (0.5 * size);
	gl_Position = ViewProjection * vec4(world, 1.0);
	v_Color = ParticleColor(emitter, t);

	vec2 frames = max(vec2(emitter.Counts.yz), vec2(1.0));
	float frame = min(floor(t * frames.x * frames.y), frames.x * frames.y - 1.0);
	// File images load flipped (row 0 = the bottom), so v = 1 is the top and frames count rows from it.
	vec2 cell = vec2(mod(frame, frames.x), frames.y - 1.0 - floor(frame / frames.x));
	v_TexCoord = (cell + vec2(corner.x * 0.5 + 0.5, corner.y * 0.5 + 0.5)) / frames;
}

#type fragment
#version 450

layout(std140, binding = 0) uniform ParticleDraw
{
	mat4 ViewProjection;
	mat4 InverseViewProjection;
	vec4 CameraRight;
	vec4 CameraUp;
	uvec4 Batch;
	vec4 Target;
};

layout(binding = 1) uniform texture2D u_Sprite;
layout(binding = 2) uniform sampler u_SpriteSampler;
layout(binding = 3) uniform texture2D u_SceneDepth;
layout(binding = 4) uniform sampler u_SceneDepthSampler;

layout(location = 0) in vec4 v_Color;
layout(location = 1) in vec2 v_TexCoord;
layout(location = 2) in vec2 v_Corner;
layout(location = 3) in float v_SoftDistance;

layout(location = 0) out vec4 o_Color;

vec3 WorldAt(vec2 uv, float depth)
{
	vec4 position = InverseViewProjection * vec4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth, 1.0);
	return position.xyz / position.w;
}

void main()
{
	vec4 color = v_Color;
	if ((Batch.w & 2u) != 0u)
	{
		float fade = clamp(1.0 - length(v_Corner), 0.0, 1.0);
		color.a *= fade * fade;
	}
	else
	{
		color *= texture(sampler2D(u_Sprite, u_SpriteSampler), v_TexCoord);
	}

	if ((Batch.w & 1u) != 0u && v_SoftDistance > 0.0)
	{
		vec2 uv = gl_FragCoord.xy * Target.xy;
		float scene = textureLod(sampler2D(u_SceneDepth, u_SceneDepthSampler), uv, 0.0).r;
		float gap = distance(WorldAt(uv, scene), WorldAt(uv, gl_FragCoord.z));
		color.a *= clamp(gap / v_SoftDistance, 0.0, 1.0);
	}

	o_Color = (Batch.w & 4u) != 0u ? vec4(color.rgb * color.a, color.a) : color;
}
