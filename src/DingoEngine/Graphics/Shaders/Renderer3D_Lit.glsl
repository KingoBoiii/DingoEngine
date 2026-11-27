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

#include <DingoEngine/Renderer3D_MeshVertex.glsl>

#type fragment
#version 450

#include <DingoEngine/Renderer3DScene.glsl>

// Mirrors Renderer3D::LitMaterialData, written for every lit material each EndScene.
layout(std140, binding = 1) uniform MaterialData
{
	vec4 EmissiveColor; // rgb = colour
	vec4 Surface;       // x = emissive strength, y = roughness, z = specular strength, w = fog target: 0 FogColor, 1 black, 2 white
};

layout(binding = 2) uniform texture2D u_Albedo;
layout(binding = 3) uniform sampler u_AlbedoSampler;

#include <DingoEngine/Shadows.glsl>
#include <DingoEngine/Volumetrics.glsl>

layout(location = 0) out vec4 o_Color;

const float PI = 3.14159265;


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
	if (VolumeParams.x > 0.5)
	{
		vec4 volume = DE_SampleVolumetricFog(ViewProjection, VolumeParams, VolumeGrid, v_WorldPosition);
		finalColor = DE_ApplyVolumetricFog(finalColor, volume, Surface.w < 0.5 ? 0 : (Surface.w < 1.5 ? 1 : 2));
	}
	// Alpha is the mesh colour's alone, never an albedo map's. Only a translucent material's meshes
	// are sorted and leave depth unwritten, so blend correctly.
	o_Color = vec4(finalColor, v_Color.a);
}
