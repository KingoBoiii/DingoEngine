#include "depch.h"
#include "DingoEngine/Graphics/PostProcess.h"
#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Graphics/FullscreenPass.h"

#include <array>
#include <cmath>
#include <cstring>

namespace Dingo
{

	namespace
	{
		// std140, mirrored by ToneMapData in PostToneMap.glsl.
		struct ToneMapData
		{
			glm::vec4 Tone{ 0.0f };  // x = operator, y = exposure multiplier, z = knee, w = white point
			glm::vec4 Bloom{ 0.0f }; // x = intensity
		};

		// std140, mirrored by BloomData in PostBloom.glsl.
		struct BloomData
		{
			glm::vec4 Source{ 0.0f };    // xy = one source texel in UV, z = upsample radius
			glm::vec4 Threshold{ 0.0f }; // x = threshold, y = knee
		};

		constexpr uint32_t k_BloomLevels = 6;

		uint32_t BloomLevelSize(uint32_t size, uint32_t level)
		{
			return std::max(size >> (level + 1), 1u);
		}

		glm::vec4 TexelOf(const Framebuffer* framebuffer, float z = 0.0f)
		{
			return glm::vec4(1.0f / static_cast<float>(framebuffer->GetWidth()), 1.0f / static_cast<float>(framebuffer->GetHeight()), z, 0.0f);
		}

		float FiniteOr(float value, float fallback)
		{
			return std::isfinite(value) ? value : fallback;
		}

		// SetUniform spends one of the material buffer's writes for the frame, so only a change does.
		template<typename T>
		void SetUniformIfChanged(Material* material, const T& data)
		{
			const std::vector<uint8_t>& current = material->GetUniformCPUData();
			if (current.size() == sizeof(T) && std::memcmp(current.data(), &data, sizeof(T)) == 0)
				return;
			material->SetUniform(data);
		}
	}

	struct PostProcessStack::Data
	{
		// Level 0 is half the scene target's size, each next one half again (at least 1 x 1).
		struct BloomChain
		{
			std::array<Framebuffer*, k_BloomLevels> Levels = {};
			std::array<Material*, k_BloomLevels> Down = {};   // Down[0] filters the scene into level 0
			std::array<Material*, k_BloomLevels - 1> Up = {}; // Up[i] adds level i + 1 into level i
		};

		struct SceneTarget
		{
			Framebuffer* Target = nullptr;
			Material* ToneMap = nullptr; // samples this target alone, so its pipelines are built once
			std::unique_ptr<BloomChain> Bloom;
			bool ToneMapBlooms = false;  // its slot 1 holds Bloom's level 0 rather than black
			uint64_t LastFrame = 0;
		};

		// One per output size, kept while it is used: two scenes of a frame drawing into targets of
		// different sizes (a viewport and a minimap) each keep theirs instead of resizing one back and
		// forth.
		std::vector<SceneTarget> Targets;
		std::unique_ptr<Internal::FullscreenShader> ToneMapShader;
		std::unique_ptr<Internal::FullscreenShader> BloomPrefilterShader;
		std::unique_ptr<Internal::FullscreenShader> BloomDownsampleShader;
		std::unique_ptr<Internal::FullscreenShader> BloomUpsampleShader;
		// What the tone map's bloom slot samples while bloom is off, so its binding set stays complete.
		Texture* Black = nullptr;

		bool Active = false;
		bool Skipped = false;
		Framebuffer* Caller = nullptr;
		Framebuffer* Current = nullptr;
		SceneTarget* CurrentTarget = nullptr; // into Targets, which nothing grows between Begin and End
		PostProcessSettings Settings;

		Statistics Stats;
		uint64_t StatsFrame = 0;
		bool NestedWarned = false;

		static constexpr uint64_t k_MaxIdleFrames = 300;

		static void ReleaseBloom(BloomChain& bloom)
		{
			for (Material*& material : bloom.Down)
				DestroyAndDelete(material);
			for (Material*& material : bloom.Up)
				DestroyAndDelete(material);
			for (Framebuffer*& level : bloom.Levels)
				DestroyAndDelete(level);
		}

		static void Release(SceneTarget& target)
		{
			if (target.Bloom)
				ReleaseBloom(*target.Bloom);
			target.Bloom.reset();
			DestroyAndDelete(target.ToneMap);
			DestroyAndDelete(target.Target);
		}

		static void ResizeBloom(SceneTarget& target)
		{
			if (!target.Bloom)
				return;
			for (uint32_t level = 0; level < k_BloomLevels; ++level)
				target.Bloom->Levels[level]->Resize(BloomLevelSize(target.Target->GetWidth(), level), BloomLevelSize(target.Target->GetHeight(), level));
		}

		BloomChain& EnsureBloom(SceneTarget& target)
		{
			if (target.Bloom)
				return *target.Bloom;

			if (!BloomPrefilterShader)
			{
				BloomPrefilterShader = std::make_unique<Internal::FullscreenShader>("PostBloomPrefilter", "PostBloom.glsl", std::vector<ShaderDefine>{ { "DE_BLOOM_PREFILTER", "" } });
				BloomDownsampleShader = std::make_unique<Internal::FullscreenShader>("PostBloomDownsample", "PostBloom.glsl", std::vector<ShaderDefine>{ { "DE_BLOOM_DOWNSAMPLE", "" } });
				BloomUpsampleShader = std::make_unique<Internal::FullscreenShader>("PostBloomUpsample", "PostBloom.glsl", std::vector<ShaderDefine>{ { "DE_BLOOM_UPSAMPLE", "" } });
			}

			target.Bloom = std::make_unique<BloomChain>();
			BloomChain& bloom = *target.Bloom;
			for (uint32_t level = 0; level < k_BloomLevels; ++level)
			{
				bloom.Levels[level] = Framebuffer::Create(FramebufferParams()
					.SetDebugName(std::format("Post bloom {}", level))
					.SetWidth(static_cast<int32_t>(BloomLevelSize(target.Target->GetWidth(), level)))
					.SetHeight(static_cast<int32_t>(BloomLevelSize(target.Target->GetHeight(), level)))
					.AddAttachment({ TextureFormat::R11G11B10F }));
			}

			for (uint32_t level = 0; level < k_BloomLevels; ++level)
			{
				const Internal::FullscreenShader& shader = level == 0 ? *BloomPrefilterShader : *BloomDownsampleShader;
				Material* down = shader.CreateMaterial(std::format("Post bloom down {}", level));
				down->SetTexture(0, level == 0 ? target.Target->GetAttachment(0) : bloom.Levels[level - 1]->GetAttachment(0));
				down->SetSampler(0, Renderer::GetClampSampler());
				bloom.Down[level] = down;
			}

			for (uint32_t level = 0; level + 1 < k_BloomLevels; ++level)
			{
				Material* up = BloomUpsampleShader->CreateMaterial(std::format("Post bloom up {}", level), BlendMode::Additive);
				up->SetTexture(0, bloom.Levels[level + 1]->GetAttachment(0));
				up->SetSampler(0, Renderer::GetClampSampler());
				bloom.Up[level] = up;
			}
			return bloom;
		}

		void DrawBloom(SceneTarget& target, const BloomSettings& settings)
		{
			DE_PROFILE_SCOPE("PostProcessStack::Bloom");
			Renderer::BeginGpuTimer("Bloom");

			BloomChain& bloom = EnsureBloom(target);
			const float threshold = std::max(FiniteOr(settings.Threshold, 1.0f), 0.0f);
			const float knee = std::max(FiniteOr(settings.Knee, 0.1f), 1e-4f);
			const float radius = std::clamp(FiniteOr(settings.Radius, 1.0f), 0.0f, 8.0f);

			for (uint32_t level = 0; level < k_BloomLevels; ++level)
			{
				BloomData data;
				data.Source = TexelOf(level == 0 ? target.Target : bloom.Levels[level - 1]);
				data.Threshold = glm::vec4(threshold, knee, 0.0f, 0.0f);
				SetUniformIfChanged(bloom.Down[level], data);
				Internal::DrawFullscreen(bloom.Down[level], bloom.Levels[level]);
			}

			for (uint32_t level = k_BloomLevels - 1; level-- > 0;)
			{
				BloomData data;
				data.Source = TexelOf(bloom.Levels[level + 1], radius);
				SetUniformIfChanged(bloom.Up[level], data);
				Internal::DrawFullscreen(bloom.Up[level], bloom.Levels[level]);
			}

			Renderer::EndGpuTimer();
		}

		static uint64_t TargetBytes(const SceneTarget& target)
		{
			uint64_t bytes = static_cast<uint64_t>(target.Target->GetWidth()) * target.Target->GetHeight() * (GetBytesPerPixel(TextureFormat::RGBA16F) + GetBytesPerPixel(TextureFormat::D32));
			if (target.Bloom)
			{
				for (const Framebuffer* level : target.Bloom->Levels)
					bytes += static_cast<uint64_t>(level->GetWidth()) * level->GetHeight() * GetBytesPerPixel(TextureFormat::R11G11B10F);
			}
			return bytes;
		}

		SceneTarget& AcquireTarget(uint32_t width, uint32_t height, uint64_t frame)
		{
			for (auto it = Targets.begin(); it != Targets.end();)
			{
				if (frame > it->LastFrame + k_MaxIdleFrames)
				{
					Release(*it);
					it = Targets.erase(it);
					continue;
				}
				++it;
			}

			for (SceneTarget& target : Targets)
			{
				if (target.Target->GetWidth() == width && target.Target->GetHeight() == height)
				{
					target.LastFrame = frame;
					return target;
				}
			}

			// A target no output of this frame uses yet is resized in place, so a window being dragged
			// to a new size doesn't leave one target per size behind.
			for (SceneTarget& target : Targets)
			{
				if (target.LastFrame != frame)
				{
					target.Target->Resize(width, height);
					ResizeBloom(target);
					target.LastFrame = frame;
					return target;
				}
			}

			if (!ToneMapShader)
			{
				ToneMapShader = std::make_unique<Internal::FullscreenShader>("PostToneMap", "PostToneMap.glsl");
				const uint32_t black = 0xFF000000;
				Black = Texture::CreateFromData(1, 1, &black, TextureFormat::RGBA, "Post black");
			}

			SceneTarget& target = Targets.emplace_back();
			target.Target = Framebuffer::Create(FramebufferParams()
				.SetDebugName("Post scene target")
				.SetWidth(static_cast<int32_t>(width))
				.SetHeight(static_cast<int32_t>(height))
				.AddAttachment({ TextureFormat::RGBA16F })
				.SetDepthSampleable(true));
			target.ToneMap = ToneMapShader->CreateMaterial("Post tone map");
			target.ToneMap->SetTexture(0, target.Target->GetAttachment(0));
			target.ToneMap->SetSampler(0, Renderer::GetPointSampler());
			target.ToneMap->SetTexture(1, Black);
			target.ToneMap->SetSampler(1, Renderer::GetClampSampler());
			target.LastFrame = frame;
			return target;
		}
	};

	PostProcessStack::PostProcessStack()
		: m_Data(std::make_unique<Data>())
	{
	}

	PostProcessStack::~PostProcessStack()
	{
		Shutdown();
	}

	void PostProcessStack::Shutdown()
	{
		for (Data::SceneTarget& target : m_Data->Targets)
			Data::Release(target);
		m_Data->Targets.clear();
		m_Data->ToneMapShader.reset();
		m_Data->BloomPrefilterShader.reset();
		m_Data->BloomDownsampleShader.reset();
		m_Data->BloomUpsampleShader.reset();
		DestroyAndDelete(m_Data->Black);
		m_Data->Active = false;
	}

	void PostProcessStack::Begin(const PostProcessSettings& settings)
	{
		Data& data = *m_Data;
		if (data.Active || data.Skipped)
		{
			if (!data.NestedWarned)
			{
				DE_CORE_WARN("PostProcessStack::Begin called again before End; the second Begin is ignored.");
				data.NestedWarned = true;
			}
			return;
		}

		if (!settings.Enabled || Renderer::IsFrameSkipped())
		{
			data.Skipped = true;
			return;
		}

		data.Caller = Renderer::GetRenderTarget();
		const Framebuffer* output = data.Caller ? data.Caller : Renderer::GetSwapChainFramebuffer();
		const uint32_t width = std::max(output->GetWidth(), 1u);
		const uint32_t height = std::max(output->GetHeight(), 1u);
		const uint64_t frame = Renderer::GetFrameIndex();

		Data::SceneTarget& target = data.AcquireTarget(width, height, frame);
		data.CurrentTarget = &target;
		data.Current = target.Target;
		data.Settings = settings;
		data.Active = true;

		if (data.StatsFrame != frame)
		{
			data.StatsFrame = frame;
			data.Stats.Scenes = 0;
			data.Stats.BloomScenes = 0;
		}
		++data.Stats.Scenes;
		data.Stats.Width = width;
		data.Stats.Height = height;

		Renderer::SetRenderTarget(data.Current);
	}

	void PostProcessStack::End()
	{
		Data& data = *m_Data;
		if (data.Skipped)
		{
			data.Skipped = false;
			return;
		}
		if (!data.Active)
			return;

		data.Active = false;
		Renderer::SetRenderTarget(data.Caller);

		DE_PROFILE_SCOPE("PostProcessStack::End");
		Renderer::BeginGpuTimer("Post");

		Data::SceneTarget& target = *data.CurrentTarget;
		const BloomSettings& bloom = data.Settings.Bloom;
		const bool blooms = bloom.Enabled && FiniteOr(bloom.Intensity, 0.0f) > 0.0f;
		if (blooms)
		{
			data.DrawBloom(target, bloom);
			++data.Stats.BloomScenes;
		}

		// Swapping the slot rebuilds the material's pipelines, so it moves only when bloom is switched.
		if (blooms != target.ToneMapBlooms)
		{
			target.ToneMap->SetTexture(1, blooms ? target.Bloom->Levels[0]->GetAttachment(0) : data.Black);
			target.ToneMapBlooms = blooms;
		}

		const ToneMapSettings& tone = data.Settings.Tone;
		const float knee = std::clamp(FiniteOr(tone.Knee, 0.8f), 0.0f, 0.999f);
		ToneMapData toneData;
		toneData.Tone = glm::vec4(
			static_cast<float>(tone.Operator),
			std::exp2(std::clamp(FiniteOr(tone.Exposure, 0.0f), -20.0f, 20.0f)),
			knee,
			std::max(FiniteOr(tone.WhitePoint, 4.0f), knee + 1e-3f));

		toneData.Bloom = glm::vec4(blooms ? FiniteOr(bloom.Intensity, 0.0f) : 0.0f, 0.0f, 0.0f, 0.0f);

		SetUniformIfChanged(target.ToneMap, toneData);
		Internal::DrawFullscreen(target.ToneMap, data.Caller);

		data.Stats.SceneTargets = static_cast<uint32_t>(data.Targets.size());
		data.Stats.TargetBytes = 0;
		for (const Data::SceneTarget& cached : data.Targets)
			data.Stats.TargetBytes += Data::TargetBytes(cached);

		Renderer::EndGpuTimer();
		data.Current = nullptr;
		data.CurrentTarget = nullptr;
	}

	bool PostProcessStack::IsActive() const
	{
		return m_Data->Active;
	}

	Framebuffer* PostProcessStack::GetSceneTarget() const
	{
		return m_Data->Active ? m_Data->Current : nullptr;
	}

	const PostProcessStack::Statistics& PostProcessStack::GetStatistics() const
	{
		return m_Data->Stats;
	}

}
