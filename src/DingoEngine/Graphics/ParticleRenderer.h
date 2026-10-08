#pragma once
#include "DingoEngine/Graphics/Particles.h"
#include "DingoEngine/Graphics/Renderer3D.h"

#include <glm/glm.hpp>

#include <memory>
#include <unordered_map>
#include <vector>

namespace Dingo::Internal
{

	// Every ParticleEffect alive, in creation order, for the F4 effect editor. Main thread only.
	const std::vector<ParticleEffect*>& GetLiveParticleEffects();
	// The effect's params as the fluent C++ that builds them, for pasting back into a game.
	std::string ParticleEffectToCode(const ParticleEffectParams& params);

	// Free ranges of a Renderer3D's particle pool, shared with its emitters so one released after the
	// renderer still has somewhere to go.
	struct ParticlePool
	{
		struct Range
		{
			uint32_t Base = 0;
			uint32_t Count = 0;
		};

		uint32_t Capacity = 0;
		uint32_t Used = 0;
		std::vector<Range> Free;

		explicit ParticlePool(uint32_t capacity) : Capacity(capacity) { Free.push_back({ 0, capacity }); }
		bool Allocate(uint32_t count, uint32_t& base);
		void Release(uint32_t base, uint32_t count);
	};

	// Renderer3D's GPU particles: the pool, the scene's submissions, the emit and simulate kernels and
	// the draws.
	class ParticleRenderer
	{
	public:
		explicit ParticleRenderer(uint32_t capacity);
		~ParticleRenderer();
		ParticleRenderer(const ParticleRenderer&) = delete;
		ParticleRenderer& operator=(const ParticleRenderer&) = delete;

		std::shared_ptr<ParticleEmitter> CreateEmitter(const ParticleEffect* effect);
		void Submit(ParticleEmitter& emitter, const glm::mat4& transform, float deltaTime);
		bool Owns(const ParticleEmitter& emitter) const { return emitter.m_Pool == m_Pool; }
		// Simulates the scene's emitters and draws them into the current render target, which must be
		// the one the scene's opaque pass drew into (its depth tests the particles).
		void EndScene(const glm::mat4& viewProjection, Renderer3D::Statistics& stats);
		void ClearScene() { m_Submissions.clear(); }

		GraphicsBuffer* GetPool() const { return m_PoolBuffer; }
		uint32_t GetCapacity() const { return m_Pool->Capacity; }
		uint32_t GetUsed() const { return m_Pool->Used; }

		static constexpr uint32_t k_MaxEmittersPerScene = 256;
		static constexpr uint32_t k_MaxSpawnRecords = 1024;

	private:
		void EnsureResources();
		Material* GetDrawMaterial(Texture* sprite, ParticleBlend blend, Texture* depth);

	private:
		struct Submission
		{
			std::shared_ptr<ParticleEmitter> Emitter; // until EndScene, so dropping the caller's last reference can't free it first
			glm::mat4 Transform{ 1.0f };
			float DeltaTime = 0.0f;
		};
		std::vector<Submission> m_Submissions;

		std::shared_ptr<ParticlePool> m_Pool;
		GraphicsBuffer* m_PoolBuffer = nullptr;
		GraphicsBuffer* m_EmitterBuffer = nullptr;
		GraphicsBuffer* m_SpawnBuffer = nullptr;
		Shader* m_SimulateShader = nullptr;
		Shader* m_EmitShader = nullptr;
		Shader* m_DrawShader = nullptr;
		ComputePass* m_SimulatePass = nullptr;
		ComputePass* m_EmitPass = nullptr;
		Texture* m_DotTexture = nullptr; // a white texel, for effects without a sprite

		struct DrawMaterial
		{
			Material* Material = nullptr;
			uint64_t LastFrame = 0;
		};
		struct DrawKey
		{
			const Texture* Sprite = nullptr;
			ParticleBlend Blend = ParticleBlend::Additive;
			const Texture* Depth = nullptr;
			bool operator==(const DrawKey&) const = default;
		};
		struct DrawKeyHash
		{
			size_t operator()(const DrawKey& key) const
			{
				size_t hash = std::hash<const void*>()(key.Sprite);
				hash ^= std::hash<const void*>()(key.Depth) * 0x9e3779b97f4a7c15ull;
				return hash ^ static_cast<size_t>(key.Blend);
			}
		};
		std::unordered_map<DrawKey, DrawMaterial, DrawKeyHash> m_DrawMaterials;

		uint32_t m_SceneSeed = 0;
		bool m_PoolFullWarned = false;
		bool m_EmitterOverflowWarned = false;
		bool m_SpawnOverflowWarned = false;
		bool m_ForeignEmitterWarned = false;
	};

}
