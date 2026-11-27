// The procedural sky's atmosphere model (Hillaire 2020, "A Scalable and Production Ready Sky and Atmosphere
// Rendering Technique"): include it with <DingoEngine/Atmosphere.glsl>. Written in the GLSL subset that also
// compiles as C++ over GLM (float literals carry the f suffix, no out parameters), so Environment.cpp
// evaluates the sun's transmittance with the very code the bake kernels run, and AtmosphereTests.cpp checks it.
//
// Distances are kilometres. A point is (radius from the planet centre, cosine of the angle between a ray and the
// local up). Medium coefficients are per kilometre.

#ifndef DE_ATMOSPHERE_GLSL
#define DE_ATMOSPHERE_GLSL

#ifndef DE_PI
#define DE_PI 3.14159265358979f
#endif

const float DE_PLANET_RADIUS = 6360.0f;
const float DE_ATMOSPHERE_TOP = 6460.0f;
const float DE_RAYLEIGH_HEIGHT = 8.0f;
const float DE_MIE_HEIGHT = 1.2f;
const float DE_OZONE_CENTER = 25.0f;
const float DE_OZONE_HALF_WIDTH = 15.0f;

const int DE_TRANSMITTANCE_STEPS = 40;
const float DE_TRANSMITTANCE_WIDTH = 256.0f;
const float DE_TRANSMITTANCE_HEIGHT = 64.0f;
const float DE_MULTISCATTER_SIZE = 32.0f;
const float DE_SKYVIEW_WIDTH = 192.0f;
const float DE_SKYVIEW_HEIGHT = 108.0f;

struct DE_Atmosphere
{
	vec3 RayleighScattering;
	float MieScattering;
	vec3 OzoneAbsorption;
	float MieAbsorption;
	float MieG;
};

// a = (Rayleigh scattering rgb, Mie scattering), b = (ozone absorption rgb, Mie absorption): the two vec4 a bake
// kernel gets in its push constants.
DE_Atmosphere DE_MakeAtmosphere(vec4 a, vec4 b, float mieG)
{
	DE_Atmosphere atmo;
	atmo.RayleighScattering = vec3(a.x, a.y, a.z);
	atmo.MieScattering = a.w;
	atmo.OzoneAbsorption = vec3(b.x, b.y, b.z);
	atmo.MieAbsorption = b.w;
	atmo.MieG = mieG;
	return atmo;
}

// Earth's coefficients scaled: Turbidity scales the aerosols (Mie scattering 3.996e-3 and absorption 4.4e-3).
DE_Atmosphere DE_EarthAtmosphere(float rayleighScale, float ozoneScale, float turbidity, float mieG)
{
	return DE_MakeAtmosphere(
		vec4(5.802e-3f * rayleighScale, 13.558e-3f * rayleighScale, 33.1e-3f * rayleighScale, 3.996e-3f * turbidity),
		vec4(0.65e-3f * ozoneScale, 1.881e-3f * ozoneScale, 0.085e-3f * ozoneScale, 4.4e-3f * turbidity),
		mieG);
}

struct DE_Medium
{
	vec3 Rayleigh;
	float Mie;
	vec3 Extinction;
};

// The medium at `h` kilometres above the surface.
DE_Medium DE_SampleMedium(DE_Atmosphere atmo, float h)
{
	float densityR = exp(-h / DE_RAYLEIGH_HEIGHT);
	float densityM = exp(-h / DE_MIE_HEIGHT);
	float densityO = max(1.0f - abs(h - DE_OZONE_CENTER) / DE_OZONE_HALF_WIDTH, 0.0f);
	DE_Medium medium;
	medium.Rayleigh = atmo.RayleighScattering * densityR;
	medium.Mie = atmo.MieScattering * densityM;
	medium.Extinction = medium.Rayleigh + vec3((atmo.MieScattering + atmo.MieAbsorption) * densityM) + atmo.OzoneAbsorption * densityO;
	return medium;
}

float DE_RayleighPhase(float cosTheta)
{
	return 3.0f / (16.0f * DE_PI) * (1.0f + cosTheta * cosTheta);
}

// Cornette-Shanks; cosTheta = 1 looks straight at the sun, where g > 0 peaks.
float DE_MiePhase(float g, float cosTheta)
{
	float g2 = g * g;
	float k = 3.0f / (8.0f * DE_PI) * (1.0f - g2) / (2.0f + g2);
	return k * (1.0f + cosTheta * cosTheta) / pow(max(1.0f + g2 - 2.0f * g * cosTheta, 1e-4f), 1.5f);
}

// Radius at distance t along the ray (r, mu).
float DE_RadiusAt(float r, float mu, float t)
{
	return sqrt(max(t * t + 2.0f * r * mu * t + r * r, 0.0f));
}

// Cosine of the angle between the ray and the local up at distance t.
float DE_MuAt(float r, float mu, float t)
{
	return (r * mu + t) / max(DE_RadiusAt(r, mu, t), 1e-3f);
}

// The cosine of the horizon's direction seen from radius r (at or below the planet's surface: 0).
float DE_HorizonCos(float r)
{
	return -sqrt(max((r - DE_PLANET_RADIUS) * (r + DE_PLANET_RADIUS), 0.0f)) / r;
}

bool DE_RayHitsPlanet(float r, float mu)
{
	return mu < DE_HorizonCos(r);
}

// Distance from (r, mu) to the top of the atmosphere; r must be below the top.
float DE_DistanceToTop(float r, float mu)
{
	float b = r * mu;
	float c = (r - DE_ATMOSPHERE_TOP) * (r + DE_ATMOSPHERE_TOP);
	return -b + sqrt(max(b * b - c, 0.0f));
}

// Distance from (r, mu) to the planet's surface; only meaningful when DE_RayHitsPlanet.
float DE_DistanceToGround(float r, float mu)
{
	float b = r * mu;
	float c = (r - DE_PLANET_RADIUS) * (r + DE_PLANET_RADIUS);
	return -b - sqrt(max(b * b - c, 0.0f));
}

// Where the quadratic step distribution puts segment `i` of `count`: it crowds the steps near the origin, where
// the Mie layer is thin.
float DE_StepStart(float tMax, int i, int count)
{
	float s = float(i) / float(count);
	return tMax * s * s;
}

// exp(-optical depth) from (r, mu) to the top of the atmosphere, ignoring the planet: the kernel the
// transmittance LUT stores.
vec3 DE_TransmittanceMarch(DE_Atmosphere atmo, float r, float mu)
{
	float tMax = DE_DistanceToTop(r, mu);
	vec3 depth = vec3(0.0f);
	for (int i = 0; i < DE_TRANSMITTANCE_STEPS; ++i)
	{
		float t0 = DE_StepStart(tMax, i, DE_TRANSMITTANCE_STEPS);
		float t1 = DE_StepStart(tMax, i + 1, DE_TRANSMITTANCE_STEPS);
		float tm = 0.5f * (t0 + t1);
		depth += DE_SampleMedium(atmo, DE_RadiusAt(r, mu, tm) - DE_PLANET_RADIUS).Extinction * (t1 - t0);
	}
	return exp(-depth);
}

// As above, 0 for a ray that hits the planet: the sunlight a surface or sample point under the horizon gets.
vec3 DE_TransmittanceToTop(DE_Atmosphere atmo, float r, float mu)
{
	if (DE_RayHitsPlanet(r, mu))
		return vec3(0.0f);
	return DE_TransmittanceMarch(atmo, r, mu);
}

// A parameter in [0, 1] whose end values land on the centres of the first and last texel of `size`, and back.
float DE_UnitToTexelUv(float x, float size)
{
	return (x * (size - 1.0f) + 0.5f) / size;
}

float DE_TexelUvToUnit(float uv, float size)
{
	return (uv * size - 0.5f) / (size - 1.0f);
}

// Bruneton's mapping of (r, mu) to the unit square: x = mu, y = r.
vec2 DE_TransmittanceUnit(float r, float mu)
{
	float h = sqrt((DE_ATMOSPHERE_TOP - DE_PLANET_RADIUS) * (DE_ATMOSPHERE_TOP + DE_PLANET_RADIUS));
	float rho = sqrt(max((r - DE_PLANET_RADIUS) * (r + DE_PLANET_RADIUS), 0.0f));
	float dMin = DE_ATMOSPHERE_TOP - r;
	float dMax = rho + h;
	float d = DE_DistanceToTop(r, mu);
	return vec2(clamp((d - dMin) / max(dMax - dMin, 1e-4f), 0.0f, 1.0f), clamp(rho / h, 0.0f, 1.0f));
}

// x = (r, mu) of the unit square's point, in .x the radius and in .y the cosine.
vec2 DE_TransmittanceFromUnit(vec2 unit)
{
	float h = sqrt((DE_ATMOSPHERE_TOP - DE_PLANET_RADIUS) * (DE_ATMOSPHERE_TOP + DE_PLANET_RADIUS));
	float rho = h * unit.y;
	float r = sqrt(rho * rho + DE_PLANET_RADIUS * DE_PLANET_RADIUS);
	float dMin = DE_ATMOSPHERE_TOP - r;
	float dMax = rho + h;
	float d = dMin + unit.x * (dMax - dMin);
	float mu = d < 1e-4f ? 1.0f : (h * h - rho * rho - d * d) / (2.0f * r * d);
	return vec2(r, clamp(mu, -1.0f, 1.0f));
}

vec2 DE_TransmittanceUv(float r, float mu)
{
	vec2 unit = DE_TransmittanceUnit(r, mu);
	return vec2(DE_UnitToTexelUv(unit.x, DE_TRANSMITTANCE_WIDTH), DE_UnitToTexelUv(unit.y, DE_TRANSMITTANCE_HEIGHT));
}

// The multiple-scattering LUT: x = the sun's elevation cosine mapped to [0, 1], y = height over the atmosphere's depth.
vec2 DE_MultiScatterUv(float sunCos, float h)
{
	return vec2(DE_UnitToTexelUv(sunCos * 0.5f + 0.5f, DE_MULTISCATTER_SIZE),
		DE_UnitToTexelUv(clamp(h / (DE_ATMOSPHERE_TOP - DE_PLANET_RADIUS), 0.0f, 1.0f), DE_MULTISCATTER_SIZE));
}

// The sky-view LUT, seen from radius r: x runs over the azimuth from the sun's, y over the elevation, with the
// horizon at 0.5 and both halves crowded toward it. viewCos = the view direction's y; lightViewCos = the cosine
// of the azimuth between the view and the sun.
vec2 DE_SkyViewUv(float r, float viewCos, float lightViewCos)
{
	float cosBeta = sqrt(max((r - DE_PLANET_RADIUS) * (r + DE_PLANET_RADIUS), 0.0f)) / r;
	float beta = acos(clamp(cosBeta, 0.0f, 1.0f));
	float horizon = DE_PI - beta;
	float zenith = acos(clamp(viewCos, -1.0f, 1.0f));
	vec2 uv;
	if (zenith < horizon)
		uv.y = (1.0f - sqrt(max(1.0f - zenith / horizon, 0.0f))) * 0.5f;
	else
		uv.y = sqrt(clamp((zenith - horizon) / max(beta, 1e-4f), 0.0f, 1.0f)) * 0.5f + 0.5f;
	uv.x = sqrt(clamp(0.5f - 0.5f * lightViewCos, 0.0f, 1.0f));
	return uv;
}

// The inverse of DE_SkyViewUv: (viewCos, lightViewCos).
vec2 DE_SkyViewParams(float r, vec2 uv)
{
	float cosBeta = sqrt(max((r - DE_PLANET_RADIUS) * (r + DE_PLANET_RADIUS), 0.0f)) / r;
	float beta = acos(clamp(cosBeta, 0.0f, 1.0f));
	float horizon = DE_PI - beta;
	float viewCos;
	if (uv.y < 0.5f)
	{
		float c = 1.0f - 2.0f * uv.y;
		viewCos = cos(horizon * (1.0f - c * c));
	}
	else
	{
		float c = uv.y * 2.0f - 1.0f;
		viewCos = cos(horizon + beta * c * c);
	}
	return vec2(clamp(viewCos, -1.0f, 1.0f), 1.0f - 2.0f * uv.x * uv.x);
}

// The sun's disc darkens toward its limb; x2 = (angle from the centre / disc radius) squared.
vec3 DE_SunLimb(float x2)
{
	float mu = sqrt(max(1.0f - x2, 0.0f));
	return pow(vec3(mu), vec3(0.397f, 0.503f, 0.652f));
}

#endif
