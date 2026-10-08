#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Dingo
{

	class Texture;

	namespace Internal
	{
		struct ParticlePool;
		class ParticleRenderer;
	}

	// Where an effect's particles start and which way they leave, in the emitter's space.
	enum class ParticleShape
	{
		Point,  // at the emitter, in every direction
		Sphere, // inside a sphere of radius ShapeSize.x, outward
		Cone,   // from a disc of radius ShapeSize.y, along +y within ShapeSize.x degrees of it
		Box     // inside a box of half extents ShapeSize, along +y
	};

	enum class ParticleBlend
	{
		Additive, // adds light: needs no sorting, and colours past 1 bloom
		Alpha     // over what is behind, unsorted
	};

	struct ParticleColorKey
	{
		float Time = 0.0f; // 0 at birth, 1 at death
		glm::vec4 Color{ 1.0f };
	};

	// What an effect looks like and how its particles move. Particles are unlit; a colour past 1 blooms
	// through the post chain.
	struct ParticleEffectParams
	{
		static constexpr uint32_t k_MaxColorKeys = 4;

		std::string DebugName;

		ParticleShape Shape = ParticleShape::Point;
		glm::vec3 ShapeSize{ 0.0f };

		// Particles a second while the emitter plays, and how many it emits at once when it starts.
		float Rate = 10.0f;
		uint32_t BurstOnPlay = 0;

		// Seconds and world units a second, each picked between the two per particle.
		glm::vec2 Lifetime{ 1.0f, 1.0f };
		glm::vec2 Speed{ 1.0f, 1.0f };

		glm::vec3 Gravity{ 0.0f };
		float Drag = 0.0f;            // the fraction of its speed a particle loses a second, as exp(-Drag * t)
		float InheritVelocity = 0.0f; // the part of a world-space emitter's own velocity a particle starts with
		float NoiseStrength = 0.0f;   // a swirling, divergence-free push, in world units a second squared
		float NoiseScale = 1.0f;      // its frequency: higher swirls tighter

		glm::vec2 StartSize{ 0.1f, 0.1f }; // world units across
		float EndSize = 1.0f;              // the size at death, times the start size
		glm::vec2 StartRotation{ 0.0f, 0.0f }; // degrees, picked per particle: 0..0 keeps a sprite upright, 0..360 scatters it
		glm::vec2 Spin{ 0.0f, 0.0f };          // degrees a second

		// Colour over life, up to four keys in time order: between them the colour blends, before the
		// first and after the last it holds.
		ParticleColorKey ColorKeys[k_MaxColorKeys] = { { 0.0f, glm::vec4(1.0f) }, { 1.0f, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f) } };
		uint32_t ColorKeyCount = 2;

		ParticleBlend Blend = ParticleBlend::Additive;
		// The sprite, times the colour; null draws a soft round dot. With flipbook frames it plays the
		// frames left to right, top to bottom, over each particle's life.
		Dingo::Texture* Texture = nullptr;
		uint32_t FlipbookColumns = 1;
		uint32_t FlipbookRows = 1;

		// Through the post chain, a particle fades out where it comes within this distance of what is
		// behind it, so it doesn't cut a hard line into the floor. 0 = a hard edge.
		float SoftDistance = 0.25f;

		// The most particles one emitter of the effect keeps alive: its ring in the renderer's pool. 0 =
		// Rate x the longest Lifetime x 1.1 + BurstOnPlay + 64. Spawns past it overwrite the oldest.
		uint32_t Capacity = 0;

		ParticleEffectParams& SetDebugName(const std::string& name) { DebugName = name; return *this; }
		ParticleEffectParams& SetShape(ParticleShape shape, const glm::vec3& size = glm::vec3(0.0f)) { Shape = shape; ShapeSize = size; return *this; }
		ParticleEffectParams& SetRate(float rate) { Rate = rate; return *this; }
		ParticleEffectParams& SetBurstOnPlay(uint32_t count) { BurstOnPlay = count; return *this; }
		ParticleEffectParams& SetLifetime(float shortest, float longest) { Lifetime = { shortest, longest }; return *this; }
		ParticleEffectParams& SetSpeed(float slowest, float fastest) { Speed = { slowest, fastest }; return *this; }
		ParticleEffectParams& SetGravity(const glm::vec3& gravity) { Gravity = gravity; return *this; }
		ParticleEffectParams& SetDrag(float drag) { Drag = drag; return *this; }
		ParticleEffectParams& SetInheritVelocity(float fraction) { InheritVelocity = fraction; return *this; }
		ParticleEffectParams& SetNoise(float strength, float scale) { NoiseStrength = strength; NoiseScale = scale; return *this; }
		ParticleEffectParams& SetStartSize(float smallest, float largest) { StartSize = { smallest, largest }; return *this; }
		ParticleEffectParams& SetEndSize(float factor) { EndSize = factor; return *this; }
		ParticleEffectParams& SetStartRotation(float smallest, float largest) { StartRotation = { smallest, largest }; return *this; }
		ParticleEffectParams& SetSpin(float slowest, float fastest) { Spin = { slowest, fastest }; return *this; }
		ParticleEffectParams& SetColors(std::initializer_list<ParticleColorKey> keys)
		{
			ColorKeyCount = 0;
			for (const ParticleColorKey& key : keys)
			{
				if (ColorKeyCount < k_MaxColorKeys)
					ColorKeys[ColorKeyCount++] = key;
			}
			return *this;
		}
		ParticleEffectParams& SetBlend(ParticleBlend blend) { Blend = blend; return *this; }
		ParticleEffectParams& SetTexture(Dingo::Texture* texture, uint32_t columns = 1, uint32_t rows = 1) { Texture = texture; FlipbookColumns = columns; FlipbookRows = rows; return *this; }
		ParticleEffectParams& SetSoftDistance(float distance) { SoftDistance = distance; return *this; }
		ParticleEffectParams& SetCapacity(uint32_t capacity) { Capacity = capacity; return *this; }
	};

	// An effect's look and behaviour, shared by every emitter that plays it. It holds no GPU resources;
	// SetParams takes effect on the next frame for every emitter, except Capacity, which an emitter
	// takes when it is made. It must outlive its emitters, and its texture it. Every live effect is
	// listed in the F4 Renderer tab's effect editor, which edits it in place and prints its params as code.
	class ParticleEffect
	{
	public:
		static ParticleEffect* Create(const ParticleEffectParams& params);
		~ParticleEffect();
		ParticleEffect(const ParticleEffect&) = delete;
		ParticleEffect& operator=(const ParticleEffect&) = delete;

		const ParticleEffectParams& GetParams() const { return m_Params; }
		void SetParams(const ParticleEffectParams& params) { m_Params = params; }
		// The ring an emitter of it takes: Capacity, or what the rate and lifetime need.
		uint32_t GetEmitterCapacity() const;

	private:
		explicit ParticleEffect(const ParticleEffectParams& params);

		ParticleEffectParams m_Params;
	};

	// One running instance of an effect: a ring of the renderer's particle pool, made by
	// Renderer3D::CreateParticleEmitter and drawn by Renderer3D::SubmitParticles (or, in the ECS, by a
	// ParticleEmitterComponent). Releasing the last reference returns its ring to the pool; its
	// particles vanish with it.
	class ParticleEmitter
	{
	public:
		~ParticleEmitter();
		ParticleEmitter(const ParticleEmitter&) = delete;
		ParticleEmitter& operator=(const ParticleEmitter&) = delete;

		// Spawns count particles at the next submission, from the effect's shape around the emitter, or
		// around a world-space point (an impact). Past the ring's room in one step, the rest are
		// dropped (Renderer3D::Statistics::DroppedParticleSpawns, warned once). An emitter without a ring
		// (GetCapacity() 0) ignores both.
		void Emit(uint32_t count);
		void EmitAt(const glm::vec3& worldPosition, uint32_t count);

		// A stopped emitter spawns nothing at its rate; its particles live out their lives. Starting it
		// again emits the effect's BurstOnPlay.
		void SetPlaying(bool playing);
		bool IsPlaying() const { return m_Playing; }
		void SetRateScale(float scale) { m_RateScale = scale; }
		float GetRateScale() const { return m_RateScale; }
		// World space (the default): particles stay where they were born as the emitter moves. Off, they
		// move with it.
		void SetWorldSpace(bool worldSpace) { m_WorldSpace = worldSpace; }
		bool IsWorldSpace() const { return m_WorldSpace; }

		const ParticleEffect* GetEffect() const { return m_Effect; }
		// Its ring: 0 when the pool had no room for it (Renderer3D warned), and it draws nothing.
		uint32_t GetCapacity() const { return m_Capacity; }
		// Where its ring starts in Renderer3D::GetParticlePool, in particles, for tooling.
		uint32_t GetPoolOffset() const { return m_Base; }

	private:
		ParticleEmitter(const ParticleEffect* effect, std::shared_ptr<Internal::ParticlePool> pool, uint32_t base, uint32_t capacity);

		struct Burst
		{
			glm::vec3 Position{ 0.0f };
			bool AtPosition = false;
			uint32_t Count = 0;
		};

		const ParticleEffect* m_Effect = nullptr;
		std::shared_ptr<Internal::ParticlePool> m_Pool;
		uint32_t m_Base = 0;
		uint32_t m_Capacity = 0;
		uint32_t m_Head = 0;           // the ring slot the next spawn takes
		float m_SpawnCarry = 0.0f;     // the fraction of a particle the rate has owed since the last spawn
		float m_Time = 0.0f;
		glm::vec3 m_LastPosition{ 0.0f };
		bool m_HasLastPosition = false;
		bool m_NeedsClear = true;      // its slots still hold whatever the pool's last user left there
		bool m_Playing = true;
		bool m_PlayBurstPending = true;
		float m_RateScale = 1.0f;
		bool m_WorldSpace = true;
		std::vector<Burst> m_Bursts;

		friend class Internal::ParticleRenderer;
	};

}
