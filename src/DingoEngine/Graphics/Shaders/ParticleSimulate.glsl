// Renderer3D's particle kernels, one thread a particle. DE_PARTICLE_SIMULATE ages and moves every slot
// of the scene's emitters (and kills a new emitter's leftovers); DE_PARTICLE_EMIT then writes the
// scene's new particles into their emitters' rings.

#type compute
#version 450
layout(local_size_x = 64) in;

#include <DingoEngine/ParticleCommon.glsl>

layout(std430, binding = 2) buffer ParticlePool
{
	Particle Particles[];
};

#if defined(DE_PARTICLE_EMIT)

layout(std430, binding = 1) readonly buffer ParticleSpawns
{
	ParticleSpawn Spawns[];
};

const float TAU = 6.28318531;

vec3 RandomDirection(inout uint state)
{
	float z = ParticleRandom(state) * 2.0 - 1.0;
	float angle = ParticleRandom(state) * TAU;
	float r = sqrt(max(1.0 - z * z, 0.0));
	return vec3(r * cos(angle), r * sin(angle), z);
}

void main()
{
	uint thread = gl_GlobalInvocationID.x;
	if (thread >= Header.w)
		return;

	uint low = 0u;
	uint high = Header.z - 1u;
	while (low < high)
	{
		uint middle = (low + high + 1u) / 2u;
		if (Spawns[middle].Data.w <= thread)
			low = middle;
		else
			high = middle - 1u;
	}
	ParticleSpawn spawn = Spawns[low];
	ParticleEmitter emitter = Emitters[spawn.Data.x];
	uint local = thread - spawn.Data.w;
	uint slot = emitter.Ring.x + (spawn.Data.z + local) % emitter.Ring.y;

	uint state = ParticleHash(thread * 9781u + floatBitsToUint(Frame.x));
	int kind = int(emitter.Shape.w + 0.5);
	vec3 position = vec3(0.0);
	vec3 direction = vec3(0.0, 1.0, 0.0);
	if (kind == 0)
	{
		direction = RandomDirection(state);
	}
	else if (kind == 1)
	{
		direction = RandomDirection(state);
		position = direction * emitter.Shape.x * pow(ParticleRandom(state), 1.0 / 3.0);
	}
	else if (kind == 2)
	{
		float cosine = mix(1.0, cos(radians(emitter.Shape.x)), ParticleRandom(state));
		float sine = sqrt(max(1.0 - cosine * cosine, 0.0));
		float angle = ParticleRandom(state) * TAU;
		direction = vec3(sine * cos(angle), cosine, sine * sin(angle));
		float radius = emitter.Shape.y * sqrt(ParticleRandom(state));
		float around = ParticleRandom(state) * TAU;
		position = vec3(radius * cos(around), 0.0, radius * sin(around));
	}
	else
	{
		position = (vec3(ParticleRandom(state), ParticleRandom(state), ParticleRandom(state)) * 2.0 - 1.0) * emitter.Shape.xyz;
	}
	position += spawn.Position.xyz;

	float speed = mix(emitter.Motion.x, emitter.Motion.y, ParticleRandom(state));
	float life = mix(emitter.Motion.z, emitter.Motion.w, ParticleRandom(state));
	vec3 velocity = direction * speed;
	if ((emitter.Ring.w & PARTICLE_WORLD_SPACE) != 0u)
	{
		position = (emitter.Transform * vec4(position, 1.0)).xyz;
		vec3 turned = mat3(emitter.Transform) * direction;
		velocity = (dot(turned, turned) > 0.0 ? normalize(turned) : direction) * speed + emitter.Velocity.xyz * emitter.Noise.z;
	}

	Particle particle;
	particle.PositionAge = vec4(position, 0.0);
	particle.VelocityLife = vec4(velocity, max(life, 1e-3));
	particle.Misc = vec4(mix(emitter.Size.x, emitter.Size.y, ParticleRandom(state)), ParticleRandom(state) * TAU,
		mix(emitter.Spin.x, emitter.Spin.y, ParticleRandom(state)), ParticleRandom(state));
	Particles[slot] = particle;
}

#elif defined(DE_PARTICLE_SIMULATE)

// The curl of (sin y + sin 1.3z, sin z + sin 1.3x, sin x + sin 1.3y): a flow with no sources or sinks,
// so particles swirl without bunching.
vec3 CurlNoise(vec3 p)
{
	return vec3(1.3 * cos(1.3 * p.y) - cos(p.z), 1.3 * cos(1.3 * p.z) - cos(p.x), 1.3 * cos(1.3 * p.x) - cos(p.y));
}

void main()
{
	uint thread = gl_GlobalInvocationID.x;
	if (thread >= Header.y)
		return;

	ParticleEmitter emitter = Emitters[FindEmitter(thread, 0u, Header.x)];
	uint slot = emitter.Ring.x + (thread - emitter.Ring.z);
	if ((emitter.Ring.w & PARTICLE_CLEAR) != 0u)
	{
		Particles[slot].VelocityLife.w = 0.0;
		return;
	}

	Particle particle = Particles[slot];
	float dt = emitter.Velocity.w;
	if (particle.VelocityLife.w <= 0.0 || dt <= 0.0)
		return;

	float age = particle.PositionAge.w + dt;
	if (age >= particle.VelocityLife.w)
	{
		Particles[slot].VelocityLife.w = 0.0;
		return;
	}

	vec3 velocity = particle.VelocityLife.xyz + emitter.Forces.xyz * dt;
	if (emitter.Noise.x > 0.0)
		velocity += CurlNoise(particle.PositionAge.xyz * emitter.Noise.y + vec3(emitter.Noise.w * 0.3)) * (emitter.Noise.x * dt);
	velocity *= exp(-emitter.Forces.w * dt);

	particle.PositionAge = vec4(particle.PositionAge.xyz + velocity * dt, age);
	particle.VelocityLife.xyz = velocity;
	particle.Misc.y += particle.Misc.z * dt;
	Particles[slot] = particle;
}

#endif
