#include "depch.h"
#include "DingoEngine/Graphics/ParticleRenderer.h"
#include "DingoEngine/Asset/UnmanagedShaderWatch.h"
#include "DingoEngine/Graphics/EngineShaders.h"
#include "DingoEngine/Graphics/PostProcess.h"

#include <glm/gtc/matrix_access.hpp>

#include <cmath>
#include <cstring>

namespace Dingo::Internal
{

	namespace
	{
		// std430, mirrored by ParticleCommon.glsl.
		struct ParticleState
		{
			glm::vec4 PositionAge{ 0.0f };
			glm::vec4 VelocityLife{ 0.0f };
			glm::vec4 Misc{ 0.0f };
		};

		struct ParticleEmitterRecord
		{
			glm::mat4 Transform{ 1.0f };
			glm::vec4 Velocity{ 0.0f };
			glm::uvec4 Ring{ 0 };
			glm::vec4 Shape{ 0.0f };
			glm::vec4 Motion{ 0.0f };
			glm::vec4 Forces{ 0.0f };
			glm::vec4 Noise{ 0.0f };
			glm::vec4 Size{ 0.0f };
			glm::vec4 Spin{ 0.0f };
			glm::vec4 ColorTimes{ 0.0f };
			glm::vec4 Colors[ParticleEffectParams::k_MaxColorKeys];
			glm::uvec4 Counts{ 0 };
		};

		struct ParticleEmitterHeader
		{
			glm::uvec4 Counts{ 0 }; // x = emitters, y = their slots, z = spawn records, w = particles to spawn
			glm::vec4 Frame{ 0.0f };
		};

		struct ParticleSpawnRecord
		{
			glm::uvec4 Data{ 0 };
			glm::vec4 Position{ 0.0f };
		};

		// std140, mirrored by ParticleDraw in ParticleDraw.glsl.
		struct ParticleDrawData
		{
			glm::mat4 ViewProjection{ 1.0f };
			glm::mat4 InverseViewProjection{ 1.0f };
			glm::vec4 CameraRight{ 0.0f };
			glm::vec4 CameraUp{ 0.0f };
			glm::uvec4 Batch{ 0 };
			glm::vec4 Target{ 0.0f };
		};

		static_assert(sizeof(ParticleState) == 48 && sizeof(ParticleEmitterRecord) == 288 && sizeof(ParticleEmitterHeader) == 32 &&
			sizeof(ParticleSpawnRecord) == 32 && sizeof(ParticleDrawData) == 192, "the particle records must match ParticleCommon.glsl and ParticleDraw.glsl");

		constexpr uint32_t k_FlagWorldSpace = 1;
		constexpr uint32_t k_FlagClear = 2;
		constexpr uint32_t k_DrawSoft = 1;
		constexpr uint32_t k_DrawDot = 2;
		constexpr uint32_t k_DrawAdditive = 4;
		constexpr uint32_t k_GroupSize = 64;
		constexpr uint64_t k_MaxIdleFrames = 300;

		float Finite(float value, float fallback)
		{
			return std::isfinite(value) ? value : fallback;
		}

		Shader* CreateParticleShader(const char* name, const char* define)
		{
			ShaderParams params = ShaderParams().SetName(name);
			if (define)
				params.AddDefine(define);
			Shader* shader = CreateEngineShader(params, define ? "ParticleSimulate.glsl" : "ParticleDraw.glsl");
			WatchUnmanagedShader(shader);
			return shader;
		}

		void ReleaseShader(Shader*& shader)
		{
			UnwatchUnmanagedShader(shader);
			DestroyAndDelete(shader);
		}
	}

	bool ParticlePool::Allocate(uint32_t count, uint32_t& base)
	{
		for (auto it = Free.begin(); it != Free.end(); ++it)
		{
			if (it->Count < count)
				continue;
			base = it->Base;
			it->Base += count;
			it->Count -= count;
			if (it->Count == 0)
				Free.erase(it);
			Used += count;
			return true;
		}
		return false;
	}

	void ParticlePool::Release(uint32_t base, uint32_t count)
	{
		auto it = std::lower_bound(Free.begin(), Free.end(), base, [](const Range& range, uint32_t value) { return range.Base < value; });
		it = Free.insert(it, { base, count });
		Used -= std::min(Used, count);

		// Merge with the neighbours, so a released emitter's ring and the space around it serve a larger one.
		if (it + 1 != Free.end() && it->Base + it->Count == (it + 1)->Base)
		{
			it->Count += (it + 1)->Count;
			Free.erase(it + 1);
		}
		if (it != Free.begin() && (it - 1)->Base + (it - 1)->Count == it->Base)
		{
			(it - 1)->Count += it->Count;
			Free.erase(it);
		}
	}

	ParticleRenderer::ParticleRenderer(uint32_t capacity)
		: m_Pool(std::make_shared<ParticlePool>(capacity))
	{
	}

	ParticleRenderer::~ParticleRenderer()
	{
		for (auto& [key, entry] : m_DrawMaterials)
			DestroyAndDelete(entry.Material);
		m_DrawMaterials.clear();
		DestroyAndDelete(m_SimulatePass);
		DestroyAndDelete(m_EmitPass);
		ReleaseShader(m_SimulateShader);
		ReleaseShader(m_EmitShader);
		ReleaseShader(m_DrawShader);
		DestroyAndDelete(m_PoolBuffer);
		DestroyAndDelete(m_EmitterBuffer);
		DestroyAndDelete(m_SpawnBuffer);
		DestroyAndDelete(m_DotTexture);
	}

	std::shared_ptr<ParticleEmitter> ParticleRenderer::CreateEmitter(const ParticleEffect* effect)
	{
		DE_CORE_ASSERT(effect, "CreateParticleEmitter needs an effect.");

		const uint32_t wanted = std::min(effect->GetEmitterCapacity(), m_Pool->Capacity);
		uint32_t base = 0;
		uint32_t capacity = wanted;
		if (!m_Pool->Allocate(wanted, base))
		{
			capacity = 0;
			if (!m_PoolFullWarned)
			{
				DE_CORE_WARN("Renderer3D: the particle pool ({} particles, {} in use) has no room for an emitter of '{}' ({} particles); it draws nothing. Raise Renderer3DCapabilities::MaxParticles or lower the effect's Capacity.",
					m_Pool->Capacity, m_Pool->Used, effect->GetParams().DebugName, wanted);
				m_PoolFullWarned = true;
			}
		}
		return std::shared_ptr<ParticleEmitter>(new ParticleEmitter(effect, m_Pool, base, capacity));
	}

	void ParticleRenderer::Submit(ParticleEmitter& emitter, const glm::mat4& transform, float deltaTime)
	{
		if (emitter.m_Pool != m_Pool)
		{
			if (!m_ForeignEmitterWarned)
			{
				DE_CORE_WARN("Renderer3D: a particle emitter made by another Renderer3D was submitted; it is ignored.");
				m_ForeignEmitterWarned = true;
			}
			return;
		}
		if (emitter.m_Capacity == 0 || !emitter.m_Effect)
			return;
		m_Submissions.push_back({ &emitter, transform, std::isfinite(deltaTime) ? std::max(deltaTime, 0.0f) : 0.0f });
	}

	void ParticleRenderer::EnsureResources()
	{
		if (m_PoolBuffer)
			return;

		m_PoolBuffer = GraphicsBuffer::CreateStorageBuffer(static_cast<uint64_t>(m_Pool->Capacity) * sizeof(ParticleState), "Renderer3D_ParticlePool");
		m_EmitterBuffer = GraphicsBuffer::CreateStorageBuffer(sizeof(ParticleEmitterHeader) + k_MaxEmittersPerScene * sizeof(ParticleEmitterRecord), "Renderer3D_ParticleEmitters");
		m_SpawnBuffer = GraphicsBuffer::CreateStorageBuffer(k_MaxSpawnRecords * sizeof(ParticleSpawnRecord), "Renderer3D_ParticleSpawns");

		m_SimulateShader = CreateParticleShader("Renderer3DParticleSimulate", "DE_PARTICLE_SIMULATE");
		m_EmitShader = CreateParticleShader("Renderer3DParticleEmit", "DE_PARTICLE_EMIT");
		m_DrawShader = CreateParticleShader("Renderer3DParticleDraw", nullptr);

		m_SimulatePass = ComputePass::Create(ComputePassParams().SetDebugName("Renderer3D particle simulate").SetShader(m_SimulateShader));
		m_SimulatePass->SetStorageBuffer(0, m_EmitterBuffer);
		m_SimulatePass->SetStorageBuffer(2, m_PoolBuffer);
		m_EmitPass = ComputePass::Create(ComputePassParams().SetDebugName("Renderer3D particle emit").SetShader(m_EmitShader));
		m_EmitPass->SetStorageBuffer(0, m_EmitterBuffer);
		m_EmitPass->SetStorageBuffer(1, m_SpawnBuffer);
		m_EmitPass->SetStorageBuffer(2, m_PoolBuffer);

		const uint32_t white = 0xFFFFFFFF;
		m_DotTexture = Texture::CreateFromData(1, 1, &white, TextureFormat::RGBA, "Renderer3D_ParticleWhite");
	}

	Material* ParticleRenderer::GetDrawMaterial(Texture* sprite, ParticleBlend blend, Texture* depth)
	{
		const DrawKey key{ sprite, blend, depth };
		DrawMaterial& entry = m_DrawMaterials[key];
		entry.LastFrame = Renderer::GetFrameIndex();
		if (entry.Material)
			return entry.Material;

		// One material per sprite, blend and depth source: swapping a material's texture rebuilds its pipelines.
		entry.Material = Material::Create(MaterialParams()
			.SetDebugName("Renderer3D_Particles")
			.SetShader(m_DrawShader)
			.SetCullMode(CullMode::None)
			.SetDepthTest(true)
			.SetDepthWrite(false)
			.SetBlendMode(blend == ParticleBlend::Additive ? BlendMode::Additive : BlendMode::Alpha));
		entry.Material->SetTexture(0, sprite ? sprite : m_DotTexture);
		entry.Material->SetSampler(0, Renderer::GetClampSampler());
		entry.Material->SetTexture(1, depth ? depth : m_DotTexture);
		entry.Material->SetSampler(1, Renderer::GetPointSampler());
		entry.Material->SetStorageBuffer(5, m_PoolBuffer);
		entry.Material->SetStorageBuffer(6, m_EmitterBuffer);
		return entry.Material;
	}

	void ParticleRenderer::EndScene(const glm::mat4& viewProjection, Renderer3D::Statistics& stats)
	{
		if (m_Submissions.empty())
			return;

		DE_PROFILE_SCOPE("Renderer3D::Particles");
		Renderer::BeginGpuTimer("Particles");
		EnsureResources();

		// Emitters of one draw batch (sprite and blend) sit together, so a batch is one instanced draw over
		// a run of them.
		std::stable_sort(m_Submissions.begin(), m_Submissions.end(), [](const Submission& a, const Submission& b)
		{
			const ParticleEffectParams& pa = a.Emitter->m_Effect->GetParams();
			const ParticleEffectParams& pb = b.Emitter->m_Effect->GetParams();
			if (pa.Blend != pb.Blend)
				return pa.Blend < pb.Blend;
			return std::less<const Texture*>()(pa.Texture, pb.Texture);
		});
		if (m_Submissions.size() > k_MaxEmittersPerScene)
		{
			if (!m_EmitterOverflowWarned)
			{
				DE_CORE_WARN("Renderer3D: a scene submitted {} particle emitters; only {} draw.", m_Submissions.size(), k_MaxEmittersPerScene);
				m_EmitterOverflowWarned = true;
			}
			m_Submissions.resize(k_MaxEmittersPerScene);
		}

		std::vector<uint8_t> emitterData(sizeof(ParticleEmitterHeader) + m_Submissions.size() * sizeof(ParticleEmitterRecord));
		ParticleEmitterHeader& header = *reinterpret_cast<ParticleEmitterHeader*>(emitterData.data());
		header = ParticleEmitterHeader();
		ParticleEmitterRecord* records = reinterpret_cast<ParticleEmitterRecord*>(emitterData.data() + sizeof(ParticleEmitterHeader));
		std::vector<ParticleSpawnRecord> spawns;

		uint32_t slots = 0;
		uint32_t spawnCount = 0;
		bool anySoft = false;
		for (uint32_t index = 0; index < m_Submissions.size(); ++index)
		{
			const Submission& submission = m_Submissions[index];
			ParticleEmitter& emitter = *submission.Emitter;
			const ParticleEffectParams& params = emitter.m_Effect->GetParams();
			const float dt = submission.DeltaTime;
			const glm::vec3 position(submission.Transform[3]);

			glm::vec3 velocity(0.0f);
			if (emitter.m_HasLastPosition && dt > 0.0f)
				velocity = (position - emitter.m_LastPosition) / dt;
			emitter.m_LastPosition = position;
			emitter.m_HasLastPosition = true;
			emitter.m_Time += dt;

			// What the emitter spawns this step: its rate, a starting burst, and what Emit queued.
			struct Pending
			{
				glm::vec3 Position{ 0.0f };
				uint32_t Count = 0;
			};
			std::vector<Pending> pending;
			if (emitter.m_Playing)
			{
				const float rate = std::max(Finite(params.Rate, 0.0f) * Finite(emitter.m_RateScale, 1.0f), 0.0f);
				emitter.m_SpawnCarry = std::min(emitter.m_SpawnCarry + rate * dt, static_cast<float>(emitter.m_Capacity));
				const uint32_t due = static_cast<uint32_t>(emitter.m_SpawnCarry);
				emitter.m_SpawnCarry -= static_cast<float>(due);
				uint32_t count = due;
				if (emitter.m_PlayBurstPending)
					count += params.BurstOnPlay;
				if (count > 0)
					pending.push_back({ glm::vec3(0.0f), count });
				emitter.m_PlayBurstPending = false;
			}
			const glm::mat4 toLocal = glm::inverse(submission.Transform);
			for (const ParticleEmitter::Burst& burst : emitter.m_Bursts)
				pending.push_back({ burst.AtPosition ? glm::vec3(toLocal * glm::vec4(burst.Position, 1.0f)) : glm::vec3(0.0f), burst.Count });
			emitter.m_Bursts.clear();

			uint32_t room = emitter.m_Capacity;
			for (const Pending& batch : pending)
			{
				const uint32_t count = std::min(batch.Count, room);
				stats.DroppedParticleSpawns += batch.Count - count;
				if (count == 0 || spawns.size() >= k_MaxSpawnRecords)
				{
					stats.DroppedParticleSpawns += count;
					continue;
				}
				room -= count;
				spawns.push_back({ glm::uvec4(index, count, emitter.m_Head, spawnCount), glm::vec4(batch.Position, 1.0f) });
				emitter.m_Head = (emitter.m_Head + count) % emitter.m_Capacity;
				spawnCount += count;
			}

			ParticleEmitterRecord& record = records[index];
			record = ParticleEmitterRecord();
			record.Transform = submission.Transform;
			record.Velocity = glm::vec4(velocity, dt);
			record.Ring = glm::uvec4(emitter.m_Base, emitter.m_Capacity, slots, (emitter.m_WorldSpace ? k_FlagWorldSpace : 0) | (emitter.m_NeedsClear ? k_FlagClear : 0));
			record.Shape = glm::vec4(params.ShapeSize, static_cast<float>(params.Shape));
			record.Motion = glm::vec4(Finite(params.Speed.x, 0.0f), Finite(params.Speed.y, 0.0f), std::max(Finite(params.Lifetime.x, 1.0f), 1e-3f), std::max(Finite(params.Lifetime.y, 1.0f), 1e-3f));
			record.Forces = glm::vec4(params.Gravity, std::max(Finite(params.Drag, 0.0f), 0.0f));
			record.Noise = glm::vec4(std::max(Finite(params.NoiseStrength, 0.0f), 0.0f), Finite(params.NoiseScale, 1.0f), Finite(params.InheritVelocity, 0.0f), emitter.m_Time);
			record.Size = glm::vec4(std::max(Finite(params.StartSize.x, 0.1f), 0.0f), std::max(Finite(params.StartSize.y, 0.1f), 0.0f), std::max(Finite(params.EndSize, 1.0f), 0.0f), std::max(Finite(params.SoftDistance, 0.0f), 0.0f));
			record.Spin = glm::vec4(glm::radians(Finite(params.Spin.x, 0.0f)), glm::radians(Finite(params.Spin.y, 0.0f)),
				glm::radians(Finite(params.StartRotation.x, 0.0f)), glm::radians(Finite(params.StartRotation.y, 0.0f)));
			const uint32_t keys = std::min(params.ColorKeyCount, ParticleEffectParams::k_MaxColorKeys);
			for (uint32_t key = 0; key < keys; ++key)
			{
				record.ColorTimes[key] = params.ColorKeys[key].Time;
				record.Colors[key] = params.ColorKeys[key].Color;
			}
			record.Counts = glm::uvec4(keys, std::max(params.FlipbookColumns, 1u), std::max(params.FlipbookRows, 1u), 0);

			emitter.m_NeedsClear = false;
			anySoft = anySoft || record.Size.w > 0.0f;
			slots += emitter.m_Capacity;
		}

		header.Counts = glm::uvec4(static_cast<uint32_t>(m_Submissions.size()), slots, static_cast<uint32_t>(spawns.size()), spawnCount);
		header.Frame = glm::vec4(glm::uintBitsToFloat(++m_SceneSeed * 0x9e3779b9u), 0.0f, 0.0f, 0.0f);

		Renderer::Upload(m_EmitterBuffer, emitterData.data(), emitterData.size());
		if (!spawns.empty())
			Renderer::Upload(m_SpawnBuffer, spawns.data(), spawns.size() * sizeof(ParticleSpawnRecord));

		// Age and move what is alive first, so this step's new particles start at age 0.
		Renderer::Dispatch(m_SimulatePass, (slots + k_GroupSize - 1) / k_GroupSize);
		if (spawnCount > 0)
			Renderer::Dispatch(m_EmitPass, (spawnCount + k_GroupSize - 1) / k_GroupSize);

		// Drawn into the post chain's scene target, AO goes on first so it doesn't darken them, and soft
		// particles read a copy of the depth they are tested against.
		Texture* depth = nullptr;
		PostProcessStack& post = Renderer::GetPostProcessStack();
		Framebuffer* target = Renderer::GetRenderTarget();
		if (post.IsActive() && post.GetSceneTarget() == target)
		{
			post.ApplyAmbientOcclusion();
			if (anySoft)
				depth = post.CopySceneDepth();
		}

		const Framebuffer* drawn = target ? target : Renderer::GetSwapChainFramebuffer();
		ParticleDrawData draw;
		draw.ViewProjection = viewProjection;
		draw.InverseViewProjection = glm::inverse(viewProjection);
		draw.CameraRight = glm::vec4(glm::normalize(glm::vec3(glm::row(viewProjection, 0))), 0.0f);
		draw.CameraUp = glm::vec4(glm::normalize(glm::vec3(glm::row(viewProjection, 1))), 0.0f);
		draw.Target = glm::vec4(1.0f / static_cast<float>(std::max(drawn->GetWidth(), 1u)), 1.0f / static_cast<float>(std::max(drawn->GetHeight(), 1u)), 0.0f, 0.0f);

		uint32_t first = 0;
		while (first < m_Submissions.size())
		{
			const ParticleEffectParams& params = m_Submissions[first].Emitter->m_Effect->GetParams();
			uint32_t end = first + 1;
			uint32_t instances = records[first].Ring.y;
			while (end < m_Submissions.size())
			{
				const ParticleEffectParams& next = m_Submissions[end].Emitter->m_Effect->GetParams();
				if (next.Blend != params.Blend || next.Texture != params.Texture)
					break;
				instances += records[end].Ring.y;
				++end;
			}

			draw.Batch = glm::uvec4(first, end - first, records[first].Ring.z,
				(depth ? k_DrawSoft : 0) | (params.Texture ? 0 : k_DrawDot) | (params.Blend == ParticleBlend::Additive ? k_DrawAdditive : 0));
			Material* material = GetDrawMaterial(params.Texture, params.Blend, depth);
			material->SetUniform(draw);
			Renderer::Draw(material, 6, instances);
			++stats.ParticleDrawCalls;
			first = end;
		}

		if (stats.DroppedParticleSpawns > 0 && !m_SpawnOverflowWarned)
		{
			DE_CORE_WARN("Renderer3D: {} particles couldn't spawn this scene: an emitter's ring was full, or too many bursts. Raise the effect's Capacity.", stats.DroppedParticleSpawns);
			m_SpawnOverflowWarned = true;
		}

		stats.ParticleEmitters += static_cast<uint32_t>(m_Submissions.size());
		stats.ParticleSlots += slots;
		stats.ParticlesSpawned += spawnCount;
		m_Submissions.clear();

		const uint64_t frame = Renderer::GetFrameIndex();
		for (auto it = m_DrawMaterials.begin(); it != m_DrawMaterials.end();)
		{
			if (it->second.LastFrame + k_MaxIdleFrames < frame)
			{
				DestroyAndDelete(it->second.Material);
				it = m_DrawMaterials.erase(it);
				continue;
			}
			++it;
		}

		Renderer::EndGpuTimer();
	}

}
