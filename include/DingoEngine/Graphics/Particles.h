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

	/// Where an effect's particles start and which way they leave, in the emitter's space.
	enum class ParticleShape
	{
		Point,  ///< At the emitter, in every direction.
		Sphere, ///< Inside a sphere of radius ShapeSize.x, outward.
		/// From a disc of radius ShapeSize.y, along +y within ShapeSize.x degrees of it.
		Cone,
		Box     ///< Inside a box of half extents ShapeSize, along +y.
	};

	/// How an effect's particles are composited onto the scene.
	enum class ParticleBlend
	{
		Additive, ///< Adds light: needs no sorting, and colours past 1 bloom.
		Alpha     ///< Over what is behind, unsorted.
	};

	/// One stop of an effect's colour over a particle's life.
	struct ParticleColorKey
	{
		float Time = 0.0f; ///< 0 at birth, 1 at death.
		glm::vec4 Color{ 1.0f }; ///< Linear RGBA; values past 1 are HDR and bloom.
	};

	/// What an effect looks like and how its particles move. Particles are unlit; a colour past 1
	/// blooms through the post chain.
	struct ParticleEffectParams
	{
		/// The most colour keys an effect holds.
		static constexpr uint32_t k_MaxColorKeys = 4;

		std::string DebugName; ///< Label for the effect editor and logs.

		ParticleShape Shape = ParticleShape::Point; ///< Where particles start; see ShapeSize.
		glm::vec3 ShapeSize{ 0.0f }; ///< Size of Shape, in the emitter's units; see ParticleShape.

		/// Particles a second while the emitter plays, and how many it emits at once when it
		/// starts.
		///@{
		float Rate = 10.0f;
		uint32_t BurstOnPlay = 0;
		///@}

		/// Seconds and world units a second, each picked between the two per particle.
		///@{
		glm::vec2 Lifetime{ 1.0f, 1.0f };
		glm::vec2 Speed{ 1.0f, 1.0f };
		///@}

		glm::vec3 Gravity{ 0.0f }; ///< Constant acceleration in world units a second squared.
		/// The fraction of its speed a particle loses a second, as exp(-Drag * t).
		float Drag = 0.0f;
		/// The part of a world-space emitter's own velocity a particle starts with.
		float InheritVelocity = 0.0f;
		/// A swirling, divergence-free push, in world units a second squared.
		float NoiseStrength = 0.0f;
		float NoiseScale = 1.0f; ///< The noise frequency: higher swirls tighter.

		/// Size across in world units, picked per particle (min, max).
		glm::vec2 StartSize{ 0.1f, 0.1f };
		/// The size at death, times the start size.
		float EndSize = 1.0f;
		/// Degrees, picked per particle: 0..0 keeps a sprite upright, 0..360 scatters it.
		glm::vec2 StartRotation{ 0.0f, 0.0f };
		glm::vec2 Spin{ 0.0f, 0.0f }; ///< Degrees a second, picked per particle (min, max).

		/// Colour over life, up to four keys in time order: between them the colour blends, before
		/// the first and after the last it holds.
		ParticleColorKey ColorKeys[k_MaxColorKeys] = { { 0.0f, glm::vec4(1.0f) }, { 1.0f, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f) } };
		uint32_t ColorKeyCount = 2; ///< How many of ColorKeys are used, 0 to k_MaxColorKeys.

		ParticleBlend Blend = ParticleBlend::Additive; ///< How particles composite onto the scene.
		/// The sprite, times the colour; null draws a soft round dot. With flipbook frames it plays
		/// the frames left to right, top to bottom, over each particle's life. Not owned; it must
		/// outlive the effect's emitters.
		Dingo::Texture* Texture = nullptr;
		uint32_t FlipbookColumns = 1; ///< Frames across the texture; 1 for a single sprite.
		uint32_t FlipbookRows = 1; ///< Frames down the texture; 1 for a single sprite.

		/// Through the post chain, a particle fades out where it comes within this distance of what
		/// is behind it, so it doesn't cut a hard line into the floor. 0 = a hard edge. The
		/// distance is in world units.
		float SoftDistance = 0.25f;

		/// The most particles one emitter of the effect keeps alive: its ring in the renderer's
		/// pool. 0 = Rate x the longest Lifetime x 1.1 + BurstOnPlay + 64. Spawns past it overwrite
		/// the oldest.
		uint32_t Capacity = 0;

		/// Each setter sets the field of the same name and returns *this, for chaining.
		///@{
		ParticleEffectParams& SetDebugName(const std::string& name) { DebugName = name; return *this; }
		///@}
		/// Sets Shape and ShapeSize (what the size means depends on the shape).
		ParticleEffectParams& SetShape(ParticleShape shape, const glm::vec3& size = glm::vec3(0.0f)) { Shape = shape; ShapeSize = size; return *this; }
		/// Each setter sets the field of the same name and returns *this, for chaining.
		///@{
		ParticleEffectParams& SetRate(float rate) { Rate = rate; return *this; }
		ParticleEffectParams& SetBurstOnPlay(uint32_t count) { BurstOnPlay = count; return *this; }
		///@}
		/// Sets Lifetime to the range, in seconds.
		ParticleEffectParams& SetLifetime(float shortest, float longest) { Lifetime = { shortest, longest }; return *this; }
		/// Sets Speed to the range, in world units a second.
		ParticleEffectParams& SetSpeed(float slowest, float fastest) { Speed = { slowest, fastest }; return *this; }
		/// Each setter sets the field of the same name and returns *this, for chaining.
		///@{
		ParticleEffectParams& SetGravity(const glm::vec3& gravity) { Gravity = gravity; return *this; }
		ParticleEffectParams& SetDrag(float drag) { Drag = drag; return *this; }
		ParticleEffectParams& SetInheritVelocity(float fraction) { InheritVelocity = fraction; return *this; }
		///@}
		/// Sets NoiseStrength and NoiseScale.
		ParticleEffectParams& SetNoise(float strength, float scale) { NoiseStrength = strength; NoiseScale = scale; return *this; }
		/// Sets StartSize to the range, in world units across.
		ParticleEffectParams& SetStartSize(float smallest, float largest) { StartSize = { smallest, largest }; return *this; }
		/// Sets EndSize, the size at death as a factor of the start size.
		ParticleEffectParams& SetEndSize(float factor) { EndSize = factor; return *this; }
		/// Sets StartRotation to the range, in degrees.
		ParticleEffectParams& SetStartRotation(float smallest, float largest) { StartRotation = { smallest, largest }; return *this; }
		/// Sets Spin to the range, in degrees a second.
		ParticleEffectParams& SetSpin(float slowest, float fastest) { Spin = { slowest, fastest }; return *this; }
		/// Replaces ColorKeys and sets ColorKeyCount; keys past k_MaxColorKeys are dropped.
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
		/// Each setter sets the field of the same name and returns *this, for chaining.
		///@{
		ParticleEffectParams& SetBlend(ParticleBlend blend) { Blend = blend; return *this; }
		///@}
		/// Sets Texture, FlipbookColumns and FlipbookRows; the texture is not owned.
		ParticleEffectParams& SetTexture(Dingo::Texture* texture, uint32_t columns = 1, uint32_t rows = 1) { Texture = texture; FlipbookColumns = columns; FlipbookRows = rows; return *this; }
		/// Each setter sets the field of the same name and returns *this, for chaining.
		///@{
		ParticleEffectParams& SetSoftDistance(float distance) { SoftDistance = distance; return *this; }
		ParticleEffectParams& SetCapacity(uint32_t capacity) { Capacity = capacity; return *this; }
		///@}
	};

	/// An effect's look and behaviour, shared by every emitter that plays it. It holds no GPU
	/// resources; SetParams takes effect on the next frame for every emitter, except Capacity,
	/// which an emitter takes when it is made. It must outlive its emitters, and its texture it.
	/// Every live effect is listed in the F4 Renderer tab's effect editor, which edits it in place
	/// and prints its params as code.
	class ParticleEffect
	{
	public:
		/// Makes an effect from a copy of params; the caller owns it and deletes it, after its
		/// emitters.
		[[nodiscard]] static ParticleEffect* Create(const ParticleEffectParams& params);
		/// Unlists the effect from the effect editor; emitters still using it must be gone.
		~ParticleEffect();
		/// Not copyable: emitters point at the effect.
		///@{
		ParticleEffect(const ParticleEffect&) = delete;
		ParticleEffect& operator=(const ParticleEffect&) = delete;
		///@}

		/// The effect's current parameters.
		const ParticleEffectParams& GetParams() const { return m_Params; }
		/// Replaces the parameters; every emitter sees them on its next frame, except Capacity.
		void SetParams(const ParticleEffectParams& params) { m_Params = params; }
		/// The ring an emitter of it takes: Capacity, or what the rate and lifetime need.
		uint32_t GetEmitterCapacity() const;

	private:
		explicit ParticleEffect(const ParticleEffectParams& params);

		ParticleEffectParams m_Params;
	};

	/// One running instance of an effect: a ring of the renderer's particle pool, made by
	/// Renderer3D::CreateParticleEmitter and drawn by Renderer3D::SubmitParticles (or, in the ECS,
	/// by a ParticleEmitterComponent). Releasing the last reference returns its ring to the pool;
	/// its particles vanish with it.
	class ParticleEmitter : public std::enable_shared_from_this<ParticleEmitter>
	{
	public:
		/// Returns the emitter's ring to the particle pool; its particles vanish with it.
		~ParticleEmitter();
		/// Not copyable: it owns a ring of the pool.
		///@{
		ParticleEmitter(const ParticleEmitter&) = delete;
		ParticleEmitter& operator=(const ParticleEmitter&) = delete;
		///@}

		/// Spawns count particles at the next submission, from the effect's shape around the
		/// emitter, or around a world-space point (an impact). Past the ring's room in one step,
		/// the rest are dropped (Renderer3D::Statistics::DroppedParticleSpawns, warned once). An
		/// emitter without a ring (GetCapacity() 0) ignores both.
		///@{
		void Emit(uint32_t count);
		void EmitAt(const glm::vec3& worldPosition, uint32_t count);
		///@}

		/// A stopped emitter spawns nothing at its rate; its particles live out their lives.
		/// Starting it again emits the effect's BurstOnPlay.
		void SetPlaying(bool playing);
		/// Whether the emitter spawns at its rate; true on creation.
		bool IsPlaying() const { return m_Playing; }
		/// Multiplies the effect's Rate for this emitter; 1 by default, 0 spawns nothing at the
		/// rate.
		void SetRateScale(float scale) { m_RateScale = scale; }
		/// The Rate multiplier; see SetRateScale.
		float GetRateScale() const { return m_RateScale; }
		/// World space (the default): particles stay where they were born as the emitter moves.
		/// Off, they move with it.
		void SetWorldSpace(bool worldSpace) { m_WorldSpace = worldSpace; }
		/// Whether particles stay where they were born; see SetWorldSpace.
		bool IsWorldSpace() const { return m_WorldSpace; }

		/// The effect it plays; not owned, and it must outlive the emitter.
		const ParticleEffect* GetEffect() const { return m_Effect; }
		/// Its ring: 0 when the pool had no room for it (Renderer3D warned), and it draws nothing.
		uint32_t GetCapacity() const { return m_Capacity; }
		/// Where its ring starts in Renderer3D::GetParticlePool, in particles, for tooling.
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
