// Static vertices arrive in world space (Renderer3D transforms them on the CPU while batching), so
// that vertex stage only applies the camera. With DE_SKINNED the GPU skins each vertex with the
// joint palette its instance has in the scene's skin arena (Skinning.glsl), one draw per mesh; with DE_INSTANCED it places each copy
// of a repeated mesh from its record (Instancing.glsl), one draw per mesh. The fragment stage is shared. The
// fragment stage takes the scene's shadows from Shadows.glsl at bindings 5 to 7. A static vertex
// names its mesh's DE_ObjectData record, which holds a SurfaceOverride; 0 is none, the material's own.
//
// CameraData mirrors Renderer3D::CameraData. Its first three members are a frozen prefix that
// custom material shaders declare on their own, so members are only ever appended.

#type vertex
#version 450

layout(std140, binding = 0) uniform CameraData
{
	mat4 ViewProjection;
};

layout(location = 0) out vec3 v_Normal;
layout(location = 1) out vec4 v_Color;
layout(location = 2) out vec3 v_WorldPosition;
layout(location = 3) out vec2 v_TexCoord;
layout(location = 4) flat out uint v_ObjectIndex;

// Written out under precise rather than as a matrix product, which a driver may fuse differently in two
// programs (and SPIR-V to HLSL drops precise from): every variant then projects a world position to the
// same bits, so an instanced copy covers exactly the CPU batch's pixels.
vec4 ProjectPosition(mat4 viewProjection, vec4 position)
{
	precise vec4 clip = (viewProjection[0] * position.x + viewProjection[1] * position.y) + (viewProjection[2] * position.z + viewProjection[3] * position.w);
	return clip;
}

#ifdef DE_SKINNED

#define DE_SKIN_PALETTE
#include <DingoEngine/Skinning.glsl>

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in uvec4 a_Joints;
layout(location = 4) in vec4 a_Weights;

void main()
{
	mat4 skin = SkinMatrix(a_Joints, a_Weights);
	vec4 worldPosition = SkinModel() * (skin * vec4(a_Position, 1.0));

	gl_Position     = ProjectPosition(ViewProjection, worldPosition);
	v_Normal        = SkinNormal(skin, a_Normal);
	v_Color         = SkinColor();
	v_WorldPosition = worldPosition.xyz;
	v_TexCoord      = a_TexCoord;
	v_ObjectIndex   = 0u;
}

#elif defined(DE_INSTANCED)

#define DE_INSTANCE_DRAW_PUSH
#include <DingoEngine/Instancing.glsl>

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

void main()
{
	InstanceRecord instance = GetInstance(uint(gl_InstanceIndex));
	vec3 worldPosition = InstanceWorldPosition(instance, a_Position);

	gl_Position     = ProjectPosition(ViewProjection, vec4(worldPosition, 1.0));
	v_Normal        = InstanceWorldNormal(instance, a_Normal);
	v_Color         = instance.Color;
	v_WorldPosition = worldPosition;
	v_TexCoord      = a_TexCoord;
	v_ObjectIndex   = instance.Misc.x;
}

#else

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec4 a_Color;
layout(location = 3) in vec2 a_TexCoord;
layout(location = 4) in uint a_ObjectIndex;

void main()
{
	gl_Position     = ProjectPosition(ViewProjection, vec4(a_Position, 1.0));
	v_Normal        = a_Normal;
	v_Color         = a_Color;
	v_WorldPosition = a_Position;
	v_TexCoord      = a_TexCoord;
	v_ObjectIndex   = a_ObjectIndex;
}

#endif

#type fragment
#version 450

layout(location = 0) in vec3 v_Normal;
layout(location = 1) in vec4 v_Color;
layout(location = 2) in vec3 v_WorldPosition;
layout(location = 3) in vec2 v_TexCoord;
layout(location = 4) flat in uint v_ObjectIndex;

const int MAX_DIRECTIONAL_LIGHTS = 4;
const int MAX_LOCAL_LIGHTS = 32;

struct DirectionalLight
{
	vec4 Direction; // xyz = the way the light travels
	vec4 Color;     // rgb = colour × intensity
};

// A point light is a spot light whose cone factor is always 1: scale 0, offset 1.
struct LocalLight
{
	vec4 PositionRange; // xyz = world position, w = range
	vec4 Color;         // rgb = colour × intensity, w = cone scale
	vec4 SpotDirection; // xyz = the way the cone points, w = cone offset
};

layout(std140, binding = 0) uniform CameraData
{
	mat4 ViewProjection;
	vec4 LightDirection;
	vec4 Ambient;
	vec4 CameraPosition; // w = 1: world position; w = 0: orthographic, xyz = towards the camera
	vec4 AmbientColor;   // rgb = colour × intensity
	ivec4 LightCounts;   // x = directional lights, y = point and spot lights
	DirectionalLight DirectionalLights[MAX_DIRECTIONAL_LIGHTS];
	LocalLight LocalLights[MAX_LOCAL_LIGHTS];
	vec4 FogColor;  // rgb = colour, a = max opacity
	vec4 FogParams; // x = start, y = end, z = density, w = mode: 0 none, 1 linear, 2 exp, 3 exp squared
};

// Mirrors Renderer3D::LitMaterialData, written for every lit material each EndScene.
layout(std140, binding = 1) uniform MaterialData
{
	vec4 EmissiveColor; // rgb = colour
	vec4 Surface;       // x = emissive strength, y = roughness, z = specular strength, w = fog target: 0 FogColor, 1 black, 2 white
};

// Mirrors Renderer3D::ObjectRecord.
struct ObjectRecord
{
	vec4 Emissive; // rgb = colour, a = strength
	vec4 Surface;  // x = roughness, y = specular strength
};

layout(std430, binding = 8) readonly buffer DE_ObjectData
{
	ObjectRecord Objects[];
};

layout(binding = 2) uniform texture2D u_Albedo;
layout(binding = 3) uniform sampler u_AlbedoSampler;

#include <DingoEngine/Shadows.glsl>

layout(location = 0) out vec4 o_Color;

const float PI = 3.14159265;

float FogFactor(float distance)
{
	if (FogParams.w < 1.5)
		return clamp((distance - FogParams.x) / (FogParams.y - FogParams.x), 0.0, 1.0);
	float density = FogParams.z * distance;
	return 1.0 - exp(FogParams.w < 2.5 ? -density : -density * density);
}

// Normalised Blinn-Phong: the (n + 8) / 8pi factor keeps a highlight's energy the same as roughness
// changes its size.
float Highlight(vec3 normal, vec3 toLight, vec3 toCamera, float shininess)
{
	vec3 halfway = toLight + toCamera;
	float lengthSquared = dot(halfway, halfway);
	if (lengthSquared < 1e-8)
		return 0.0;

	float nDotH = max(dot(normal, halfway * inversesqrt(lengthSquared)), 0.0);
	return (shininess + 8.0) / (8.0 * PI) * pow(nDotH, shininess);
}

void main()
{
	vec3 normal = normalize(v_Normal);
	vec3 albedo = v_Color.rgb * texture(sampler2D(u_Albedo, u_AlbedoSampler), v_TexCoord).rgb;

	vec3 emissiveColor = EmissiveColor.rgb;
	float emissiveStrength = Surface.x;
	float roughness = Surface.y;
	float specularStrength = Surface.z;
	if (v_ObjectIndex != 0u)
	{
		ObjectRecord object = Objects[v_ObjectIndex];
		emissiveColor = object.Emissive.rgb;
		emissiveStrength = object.Emissive.a;
		roughness = object.Surface.x;
		specularStrength = object.Surface.y;
	}

	bool shiny = specularStrength > 0.0;
	float shininess = exp2(10.0 * (1.0 - roughness) + 1.0);
	vec3 toCamera = CameraPosition.w > 0.5 ? normalize(CameraPosition.xyz - v_WorldPosition) : CameraPosition.xyz;

	vec3 lighting = AmbientColor.rgb;
	vec3 specular = vec3(0.0);

	int directionalCount = min(LightCounts.x, MAX_DIRECTIONAL_LIGHTS);
	for (int i = 0; i < directionalCount; ++i)
	{
		vec3 toLight = normalize(-DirectionalLights[i].Direction.xyz);
		float nDotL = max(dot(normal, toLight), 0.0);
		if (i == ShadowCounts.y && nDotL > 0.0)
			nDotL *= DirectionalShadow(v_WorldPosition, normal);
		lighting += DirectionalLights[i].Color.rgb * nDotL;
		if (shiny && nDotL > 0.0)
			specular += DirectionalLights[i].Color.rgb * nDotL * Highlight(normal, toLight, toCamera, shininess);
	}

	int localCount = min(LightCounts.y, MAX_LOCAL_LIGHTS);
	for (int i = 0; i < localCount; ++i)
	{
		LocalLight light = LocalLights[i];
		vec3 toLight = light.PositionRange.xyz - v_WorldPosition;
		float distanceSquared = dot(toLight, toLight);
		float rangeSquared = light.PositionRange.w * light.PositionRange.w;
		if (!(distanceSquared < rangeSquared))
			continue;

		toLight *= inversesqrt(max(distanceSquared, 1e-8));
		float falloff = 1.0 - distanceSquared / rangeSquared;
		float cone = clamp(dot(-toLight, light.SpotDirection.xyz) * light.Color.w + light.SpotDirection.w, 0.0, 1.0);
		float nDotL = max(dot(normal, toLight), 0.0);
		if (ShadowCounts.w > 0 && nDotL > 0.0 && cone > 0.0)
			nDotL *= LocalLightShadow(i, v_WorldPosition, normal);
		lighting += light.Color.rgb * nDotL * (falloff * falloff) * (cone * cone);
		if (shiny && nDotL > 0.0)
			specular += light.Color.rgb * nDotL * (falloff * falloff) * (cone * cone) * Highlight(normal, toLight, toCamera, shininess);
	}

	vec3 finalColor = albedo * lighting + specular * specularStrength;
	finalColor += emissiveColor * emissiveStrength;
	if (ShadowCounts.z != 0)
		finalColor *= ShadowCascadeTint(v_WorldPosition);
	if (FogParams.w > 0.5 && CameraPosition.w > 0.5)
	{
		// What leaves the target as the fog left it under the material's blend mode: Additive adds
		// nothing, Multiply multiplies by white.
		vec3 fogTarget = Surface.w < 0.5 ? FogColor.rgb : vec3(Surface.w < 1.5 ? 0.0 : 1.0);
		finalColor = mix(finalColor, fogTarget, FogFactor(distance(CameraPosition.xyz, v_WorldPosition)) * FogColor.a);
	}
	// Alpha is the mesh colour's alone, never an albedo map's. Only a translucent material's meshes
	// are sorted and leave depth unwritten, so blend correctly.
	o_Color = vec4(finalColor, v_Color.a);
}
