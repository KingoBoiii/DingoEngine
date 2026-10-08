// Renderer3D's GPU particles: the pool's particle, the per-scene emitter and spawn records, the
// scene's emitter block (readonly, at DE_PARTICLE_EMITTERS_BINDING) and helpers. Mirrors
// Internal::ParticleState, ParticleEmitterRecord and ParticleSpawnRecord in ParticleRenderer.cpp.

struct Particle
{
	vec4 PositionAge;  // xyz = position (world, or the emitter's space), w = age in seconds
	vec4 VelocityLife; // xyz = velocity, w = lifetime (0 = dead)
	vec4 Misc;         // x = start size, y = rotation, z = spin, w = a random number of its own
};

struct ParticleEmitter
{
	mat4 Transform;
	vec4 Velocity;   // xyz = the emitter's own velocity, w = this step's delta time
	uvec4 Ring;      // x = first pool slot, y = slots, z = its first thread of the scene, w = flags (1 world space, 2 clear the ring)
	vec4 Shape;      // xyz = size, w = kind: 0 point, 1 sphere, 2 cone, 3 box
	vec4 Motion;     // xy = speed range, zw = lifetime range
	vec4 Forces;     // xyz = gravity, w = drag
	vec4 Noise;      // x = strength, y = scale, z = inherited velocity, w = the emitter's time
	vec4 Size;       // xy = start size range, z = end size factor, w = soft distance
	vec4 Spin;       // xy = spin range (radians a second), zw = start rotation range (radians)
	vec4 ColorTimes; // the colour keys' times
	vec4 Colors[4];
	uvec4 Counts;    // x = colour keys, yz = flipbook columns, rows
};

struct ParticleSpawn
{
	uvec4 Data;    // x = emitter, y = count, z = the ring index of its first particle, w = its first thread
	vec4 Position; // xyz = where in the emitter's space the shape is centred
};

#ifndef DE_PARTICLE_EMITTERS_BINDING
#define DE_PARTICLE_EMITTERS_BINDING 0
#endif

layout(std430, binding = DE_PARTICLE_EMITTERS_BINDING) readonly buffer ParticleEmitters
{
	uvec4 Header; // x = emitters, y = their slots, z = spawn records, w = particles to spawn
	vec4 Frame;   // x = a random seed for the scene
	ParticleEmitter Emitters[];
};

const uint PARTICLE_WORLD_SPACE = 1u;
const uint PARTICLE_CLEAR = 2u;

uint ParticleHash(uint x)
{
	x ^= x >> 16;
	x *= 0x7feb352du;
	x ^= x >> 15;
	x *= 0x846ca68bu;
	x ^= x >> 16;
	return x;
}

float ParticleRandom(inout uint state)
{
	state = ParticleHash(state);
	return float(state >> 8) * (1.0 / 16777216.0);
}

// The emitter in [first, first + count) whose threads hold thread: the last one starting at or before it.
uint FindEmitter(uint thread, uint first, uint count)
{
	uint low = first;
	uint high = first + count - 1u;
	while (low < high)
	{
		uint middle = (low + high + 1u) / 2u;
		if (Emitters[middle].Ring.z <= thread)
			low = middle;
		else
			high = middle - 1u;
	}
	return low;
}

vec4 ParticleColor(ParticleEmitter emitter, float t)
{
	uint keys = min(emitter.Counts.x, 4u);
	if (keys == 0u)
		return vec4(1.0);
	vec4 color = emitter.Colors[0];
	for (uint i = 1u; i < keys; ++i)
	{
		float start = emitter.ColorTimes[i - 1u];
		float end = emitter.ColorTimes[i];
		if (t >= end)
			color = emitter.Colors[i];
		else if (t > start)
			color = mix(emitter.Colors[i - 1u], emitter.Colors[i], (t - start) / max(end - start, 1e-5));
	}
	return color;
}
