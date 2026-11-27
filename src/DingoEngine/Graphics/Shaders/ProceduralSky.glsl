// The procedural sky's bake kernels (Environment::CreateProcedural), one variant per define, each declaring only
// the bindings it uses; the model and its mappings are in Atmosphere.glsl. DE_SKY_TRANSMITTANCE fills the 256 x 64
// transmittance LUT; DE_SKY_MULTISCATTER the 32 x 32 multiple-scattering LUT (one 64-thread group a texel, a
// direction a thread); DE_SKY_VIEW the 192 x 108 sky-view LUT, ground included; DE_SKY_CUBE resamples that into
// mip 0 of the environment cube (z = face) and adds the night floor. Every variant reads the same push block, so
// a pass only re-pushes it. Includes sit at the top: the expander pastes each file once, before #if is evaluated.

#type compute
#version 450

#include <DingoEngine/CubeMap.glsl>
#include <DingoEngine/Atmosphere.glsl>

layout(push_constant) uniform PushData
{
	vec4 P_Rayleigh;     // rgb Rayleigh scattering per km, w Mie scattering
	vec4 P_Ozone;        // rgb ozone absorption, w Mie absorption
	vec4 P_Ground;       // rgb ground albedo (linear), w Mie anisotropy
	vec4 P_Sun;          // xyz direction toward the sun, w observer radius in km
	vec4 P_Illuminance;  // rgb sun illuminance at the top of the atmosphere, w 1 = multiple scattering on
	vec4 P_Night;        // rgb night floor
	uvec4 P_Size;        // x = edge of the cube face
	vec4 P_Reserved;
};

#if defined(DE_SKY_TRANSMITTANCE)

layout(local_size_x = 32, local_size_y = 8, local_size_z = 1) in;
layout(binding = 3, rgba16f) uniform writeonly image2D u_Target;

void main()
{
	uvec2 id = gl_GlobalInvocationID.xy;
	if (id.x >= uint(DE_TRANSMITTANCE_WIDTH) || id.y >= uint(DE_TRANSMITTANCE_HEIGHT))
		return;

	vec2 unit = vec2(float(id.x) / (DE_TRANSMITTANCE_WIDTH - 1.0), float(id.y) / (DE_TRANSMITTANCE_HEIGHT - 1.0));
	vec2 point = DE_TransmittanceFromUnit(unit);
	DE_Atmosphere atmo = DE_MakeAtmosphere(P_Rayleigh, P_Ozone, 0.0);
	imageStore(u_Target, ivec2(id), vec4(DE_TransmittanceMarch(atmo, point.x, point.y), 1.0));
}

#elif defined(DE_SKY_MULTISCATTER)

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;
layout(binding = 1) uniform texture2D u_Transmittance;
layout(binding = 2) uniform sampler u_Sampler;
layout(binding = 3, rgba16f) uniform writeonly image2D u_Target;

const int k_Steps = 20;

shared vec3 s_InScatter[64];
shared vec3 s_Albedo[64];

vec3 SunTransmittance(float r, float mu)
{
	if (DE_RayHitsPlanet(r, mu))
		return vec3(0.0);
	return textureLod(sampler2D(u_Transmittance, u_Sampler), DE_TransmittanceUv(r, mu), 0.0).rgb;
}

void main()
{
	uint t = gl_LocalInvocationIndex;
	vec2 unit = vec2(gl_WorkGroupID.xy) / (DE_MULTISCATTER_SIZE - 1.0);
	float sunCos = unit.x * 2.0 - 1.0;
	float height = unit.y * (DE_ATMOSPHERE_TOP - DE_PLANET_RADIUS);
	float r = DE_PLANET_RADIUS + height;
	vec3 sunDir = vec3(sqrt(max(1.0 - sunCos * sunCos, 0.0)), sunCos, 0.0);
	DE_Atmosphere atmo = DE_MakeAtmosphere(P_Rayleigh, P_Ozone, 0.0);

	float theta = 2.0 * DE_PI * (0.5 + float(t / 8u)) / 8.0;
	float cosPhi = 1.0 - 2.0 * (0.5 + float(t % 8u)) / 8.0;
	float sinPhi = sqrt(max(1.0 - cosPhi * cosPhi, 0.0));
	vec3 dir = vec3(sinPhi * cos(theta), cosPhi, sinPhi * sin(theta));

	bool hitsGround = DE_RayHitsPlanet(r, dir.y);
	float tMax = hitsGround ? DE_DistanceToGround(r, dir.y) : DE_DistanceToTop(r, dir.y);
	vec3 origin = vec3(0.0, r, 0.0);

	vec3 inScatter = vec3(0.0);
	vec3 albedo = vec3(0.0);
	vec3 throughput = vec3(1.0);
	for (int i = 0; i < k_Steps; ++i)
	{
		float t0 = DE_StepStart(tMax, i, k_Steps);
		float t1 = DE_StepStart(tMax, i + 1, k_Steps);
		float dt = t1 - t0;
		vec3 pos = origin + dir * (0.5 * (t0 + t1));
		float rr = length(pos);
		DE_Medium medium = DE_SampleMedium(atmo, rr - DE_PLANET_RADIUS);
		vec3 scattering = medium.Rayleigh + vec3(medium.Mie);
		vec3 stepT = exp(-medium.Extinction * dt);
		vec3 sourceRatio = mix(vec3(dt), (vec3(1.0) - stepT) / max(medium.Extinction, vec3(1e-7)), step(vec3(1e-7), medium.Extinction));
		vec3 sunT = SunTransmittance(rr, dot(pos / rr, sunDir));
		inScatter += throughput * sunT * scattering * (0.25 / DE_PI) * sourceRatio;
		albedo += throughput * scattering * sourceRatio;
		throughput *= stepT;
	}
	if (hitsGround)
	{
		float groundCos = dot(vec3(0.0, 1.0, 0.0), sunDir);
		inScatter += throughput * SunTransmittance(DE_PLANET_RADIUS, groundCos) * max(groundCos, 0.0) * P_Ground.rgb / DE_PI;
	}

	s_InScatter[t] = inScatter;
	s_Albedo[t] = albedo;
	memoryBarrierShared();
	barrier();

	for (uint stride = 32u; stride > 0u; stride >>= 1u)
	{
		if (t < stride)
		{
			s_InScatter[t] += s_InScatter[t + stride];
			s_Albedo[t] += s_Albedo[t + stride];
		}
		memoryBarrierShared();
		barrier();
	}

	if (t == 0u)
	{
		vec3 secondOrder = s_InScatter[0] / 64.0;
		vec3 scatterFraction = s_Albedo[0] / 64.0;
		imageStore(u_Target, ivec2(gl_WorkGroupID.xy), vec4(secondOrder / (vec3(1.0) - scatterFraction), 1.0));
	}
}

#elif defined(DE_SKY_VIEW)

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(binding = 1) uniform texture2D u_Transmittance;
layout(binding = 5) uniform texture2D u_MultiScatter;
layout(binding = 2) uniform sampler u_Sampler;
layout(binding = 3, rgba16f) uniform writeonly image2D u_Target;

const int k_Steps = 30;

vec3 SunTransmittance(float r, float mu)
{
	if (DE_RayHitsPlanet(r, mu))
		return vec3(0.0);
	return textureLod(sampler2D(u_Transmittance, u_Sampler), DE_TransmittanceUv(r, mu), 0.0).rgb;
}

vec3 MultiScatter(float sunCos, float height)
{
	return textureLod(sampler2D(u_MultiScatter, u_Sampler), DE_MultiScatterUv(sunCos, height), 0.0).rgb * P_Illuminance.w;
}

void main()
{
	uvec2 id = gl_GlobalInvocationID.xy;
	if (id.x >= uint(DE_SKYVIEW_WIDTH) || id.y >= uint(DE_SKYVIEW_HEIGHT))
		return;

	float r = P_Sun.w;
	vec2 uv = (vec2(id) + 0.5) / vec2(DE_SKYVIEW_WIDTH, DE_SKYVIEW_HEIGHT);
	vec2 angles = DE_SkyViewParams(r, uv);
	float sinView = sqrt(max(1.0 - angles.x * angles.x, 0.0));
	vec3 dir = vec3(sinView * angles.y, angles.x, sinView * sqrt(max(1.0 - angles.y * angles.y, 0.0)));
	float sunCosZenith = P_Sun.y;
	vec3 sunDir = vec3(sqrt(max(1.0 - sunCosZenith * sunCosZenith, 0.0)), sunCosZenith, 0.0);

	DE_Atmosphere atmo = DE_MakeAtmosphere(P_Rayleigh, P_Ozone, P_Ground.w);
	vec3 illuminance = P_Illuminance.rgb;
	float cosTheta = dot(dir, sunDir);
	float phaseR = DE_RayleighPhase(cosTheta);
	float phaseM = DE_MiePhase(P_Ground.w, cosTheta);

	bool hitsGround = DE_RayHitsPlanet(r, dir.y);
	float tMax = hitsGround ? DE_DistanceToGround(r, dir.y) : DE_DistanceToTop(r, dir.y);
	vec3 origin = vec3(0.0, r, 0.0);

	vec3 radiance = vec3(0.0);
	vec3 throughput = vec3(1.0);
	for (int i = 0; i < k_Steps; ++i)
	{
		float t0 = DE_StepStart(tMax, i, k_Steps);
		float t1 = DE_StepStart(tMax, i + 1, k_Steps);
		float dt = t1 - t0;
		vec3 pos = origin + dir * (0.5 * (t0 + t1));
		float rr = length(pos);
		float sunCos = dot(pos / rr, sunDir);
		DE_Medium medium = DE_SampleMedium(atmo, rr - DE_PLANET_RADIUS);
		vec3 scattering = medium.Rayleigh + vec3(medium.Mie);
		vec3 stepT = exp(-medium.Extinction * dt);
		vec3 source = illuminance * (SunTransmittance(rr, sunCos) * (medium.Rayleigh * phaseR + vec3(medium.Mie * phaseM))
			+ MultiScatter(sunCos, rr - DE_PLANET_RADIUS) * scattering);
		vec3 sourceRatio = mix(vec3(dt), (vec3(1.0) - stepT) / max(medium.Extinction, vec3(1e-7)), step(vec3(1e-7), medium.Extinction));
		radiance += throughput * source * sourceRatio;
		throughput *= stepT;
	}
	if (hitsGround)
	{
		vec3 ground = origin + dir * tMax;
		float groundCos = dot(normalize(ground), sunDir);
		radiance += throughput * P_Ground.rgb * illuminance
			* (SunTransmittance(DE_PLANET_RADIUS, groundCos) * max(groundCos, 0.0) / DE_PI + MultiScatter(groundCos, 0.0));
	}
	imageStore(u_Target, ivec2(id), vec4(min(radiance, vec3(60000.0)), 1.0));
}

#elif defined(DE_SKY_CUBE)

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(binding = 1) uniform texture2D u_SkyView;
layout(binding = 2) uniform sampler u_Sampler;
layout(binding = 3, rgba16f) uniform writeonly image2DArray u_Target;

void main()
{
	uvec3 id = gl_GlobalInvocationID;
	if (id.x >= P_Size.x || id.y >= P_Size.x)
		return;

	vec2 uv = (vec2(id.xy) + 0.5) / float(P_Size.x);
	vec3 dir = DE_CubeFaceDirection(id.z, uv);

	vec2 sunPlane = P_Sun.xz;
	float sunLength = length(sunPlane);
	vec2 sunAzimuth = sunLength > 1e-4 ? sunPlane / sunLength : vec2(1.0, 0.0);
	vec2 viewPlane = dir.xz;
	float viewLength = length(viewPlane);
	vec2 viewAzimuth = viewLength > 1e-4 ? viewPlane / viewLength : sunAzimuth;

	vec2 lookup = DE_SkyViewUv(P_Sun.w, dir.y, dot(viewAzimuth, sunAzimuth));
	vec3 sky = textureLod(sampler2D(u_SkyView, u_Sampler), lookup, 0.0).rgb;
	imageStore(u_Target, ivec3(id), vec4(sky + P_Night.rgb, 1.0));
}

#endif
