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

		// std140, mirrored by AmbientOcclusionData in PostAmbientOcclusion.glsl.
		struct AmbientOcclusionData
		{
			glm::mat4 InverseProjection{ 1.0f };
			glm::vec4 Params{ 0.0f }; // x = radius, y = intensity / radius^6, z = bias, w = power
			glm::vec4 Target{ 0.0f }; // xy = one texel of the pass's target in UV, z = its pixels per unit at distance 1, w = 1 orthographic
			glm::vec4 Blur{ 0.0f };   // xy = the blur's step in UV, zw = one depth texel in UV
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

		// Raw takes the samples and, after the blur's two passes (through Blurred), the result.
		struct AmbientOcclusionChain
		{
			Framebuffer* Raw = nullptr;
			Framebuffer* Blurred = nullptr;
			Material* Sample = nullptr;
			Material* BlurAcross = nullptr;
			Material* BlurDown = nullptr;
			Material* Apply = nullptr;
			bool HalfResolution = true;
		};

		struct SceneTarget
		{
			Framebuffer* Target = nullptr;
			Material* ToneMap = nullptr; // samples this target alone, so its pipelines are built once
			std::unique_ptr<BloomChain> Bloom;
			std::unique_ptr<AmbientOcclusionChain> AmbientOcclusion;
			Framebuffer* DepthCopy = nullptr;
			Material* DepthCopyMaterial = nullptr;
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
		std::unique_ptr<Internal::FullscreenShader> AmbientOcclusionSampleShader;
		std::unique_ptr<Internal::FullscreenShader> AmbientOcclusionBlurShader;
		std::unique_ptr<Internal::FullscreenShader> AmbientOcclusionApplyShader;
		std::unique_ptr<Internal::FullscreenShader> DepthCopyShader;
		// What the tone map's bloom slot samples while bloom is off, so its binding set stays complete.
		Texture* Black = nullptr;

		bool Active = false;
		bool Skipped = false;
		Framebuffer* Caller = nullptr;
		Framebuffer* Current = nullptr;
		SceneTarget* CurrentTarget = nullptr; // into Targets, which nothing grows between Begin and End
		PostProcessSettings Settings;
		glm::mat4 Projection{ 1.0f };
		bool HasProjection = false;
		bool AmbientOcclusionApplied = false;
		bool DepthCopied = false;

		Statistics Stats;
		uint64_t StatsFrame = 0;
		bool NestedWarned = false;
		bool NoProjectionWarned = false;

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

		static void ReleaseAmbientOcclusion(AmbientOcclusionChain& chain)
		{
			DestroyAndDelete(chain.Sample);
			DestroyAndDelete(chain.BlurAcross);
			DestroyAndDelete(chain.BlurDown);
			DestroyAndDelete(chain.Apply);
			DestroyAndDelete(chain.Raw);
			DestroyAndDelete(chain.Blurred);
		}

		static void Release(SceneTarget& target)
		{
			if (target.Bloom)
				ReleaseBloom(*target.Bloom);
			target.Bloom.reset();
			if (target.AmbientOcclusion)
				ReleaseAmbientOcclusion(*target.AmbientOcclusion);
			target.AmbientOcclusion.reset();
			DestroyAndDelete(target.DepthCopyMaterial);
			DestroyAndDelete(target.DepthCopy);
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

		static glm::uvec2 AmbientOcclusionSize(const Framebuffer* target, bool half)
		{
			const uint32_t width = target->GetWidth();
			const uint32_t height = target->GetHeight();
			return half ? glm::uvec2(std::max((width + 1) / 2, 1u), std::max((height + 1) / 2, 1u)) : glm::uvec2(width, height);
		}

		static void ResizeAmbientOcclusion(SceneTarget& target)
		{
			if (!target.AmbientOcclusion)
				return;
			AmbientOcclusionChain& chain = *target.AmbientOcclusion;
			const glm::uvec2 size = AmbientOcclusionSize(target.Target, chain.HalfResolution);
			if (chain.Raw->GetWidth() == size.x && chain.Raw->GetHeight() == size.y)
				return;
			chain.Raw->Resize(size.x, size.y);
			chain.Blurred->Resize(size.x, size.y);
		}

		AmbientOcclusionChain& EnsureAmbientOcclusion(SceneTarget& target, bool half)
		{
			if (target.AmbientOcclusion)
			{
				target.AmbientOcclusion->HalfResolution = half;
				ResizeAmbientOcclusion(target);
				return *target.AmbientOcclusion;
			}

			if (!AmbientOcclusionSampleShader)
			{
				AmbientOcclusionSampleShader = std::make_unique<Internal::FullscreenShader>("PostAmbientOcclusionSample", "PostAmbientOcclusion.glsl", std::vector<ShaderDefine>{ { "DE_AO_SAMPLE", "" } });
				AmbientOcclusionBlurShader = std::make_unique<Internal::FullscreenShader>("PostAmbientOcclusionBlur", "PostAmbientOcclusion.glsl", std::vector<ShaderDefine>{ { "DE_AO_BLUR", "" } });
				AmbientOcclusionApplyShader = std::make_unique<Internal::FullscreenShader>("PostAmbientOcclusionApply", "PostAmbientOcclusion.glsl", std::vector<ShaderDefine>{ { "DE_AO_APPLY", "" } });
			}

			target.AmbientOcclusion = std::make_unique<AmbientOcclusionChain>();
			AmbientOcclusionChain& chain = *target.AmbientOcclusion;
			chain.HalfResolution = half;
			const glm::uvec2 size = AmbientOcclusionSize(target.Target, half);
			auto makeTarget = [&size](const char* name)
			{
				return Framebuffer::Create(FramebufferParams()
					.SetDebugName(name)
					.SetWidth(static_cast<int32_t>(size.x))
					.SetHeight(static_cast<int32_t>(size.y))
					.AddAttachment({ TextureFormat::R8 }));
			};
			chain.Raw = makeTarget("Post AO");
			chain.Blurred = makeTarget("Post AO blurred");

			Texture* depth = target.Target->GetDepthAttachment();
			chain.Sample = AmbientOcclusionSampleShader->CreateMaterial("Post AO sample");
			chain.Sample->SetTexture(0, depth);
			chain.Sample->SetSampler(0, Renderer::GetPointSampler());

			chain.BlurAcross = AmbientOcclusionBlurShader->CreateMaterial("Post AO blur across");
			chain.BlurAcross->SetTexture(0, chain.Raw->GetAttachment(0));
			chain.BlurAcross->SetSampler(0, Renderer::GetPointSampler());
			chain.BlurAcross->SetTexture(1, depth);
			chain.BlurAcross->SetSampler(1, Renderer::GetPointSampler());

			chain.BlurDown = AmbientOcclusionBlurShader->CreateMaterial("Post AO blur down");
			chain.BlurDown->SetTexture(0, chain.Blurred->GetAttachment(0));
			chain.BlurDown->SetSampler(0, Renderer::GetPointSampler());
			chain.BlurDown->SetTexture(1, depth);
			chain.BlurDown->SetSampler(1, Renderer::GetPointSampler());

			chain.Apply = AmbientOcclusionApplyShader->CreateMaterial("Post AO apply", BlendMode::Multiply);
			chain.Apply->SetTexture(0, chain.Raw->GetAttachment(0));
			chain.Apply->SetSampler(0, Renderer::GetClampSampler());
			return chain;
		}

		void DrawAmbientOcclusion(SceneTarget& target, const AmbientOcclusionSettings& settings, const glm::mat4& projection)
		{
			DE_PROFILE_SCOPE("PostProcessStack::AmbientOcclusion");
			Renderer::BeginGpuTimer("AO");

			AmbientOcclusionChain& chain = EnsureAmbientOcclusion(target, settings.HalfResolution);
			const float radius = std::max(FiniteOr(settings.Radius, 0.5f), 1e-3f);
			const float width = static_cast<float>(chain.Raw->GetWidth());
			const float height = static_cast<float>(chain.Raw->GetHeight());
			const bool orthographic = projection[3][3] > 0.5f;

			AmbientOcclusionData data;
			data.InverseProjection = glm::inverse(projection);
			data.Params = glm::vec4(radius, std::max(FiniteOr(settings.Intensity, 1.0f), 0.0f) / std::pow(radius, 6.0f),
				std::max(FiniteOr(settings.Bias, 0.02f), 0.0f), std::clamp(FiniteOr(settings.Power, 1.5f), 0.1f, 8.0f));
			data.Target = glm::vec4(1.0f / width, 1.0f / height, 0.5f * height * std::abs(projection[1][1]), orthographic ? 1.0f : 0.0f);
			const glm::vec2 depthTexel(1.0f / static_cast<float>(target.Target->GetWidth()), 1.0f / static_cast<float>(target.Target->GetHeight()));

			data.Blur = glm::vec4(0.0f, 0.0f, depthTexel);
			SetUniformIfChanged(chain.Sample, data);
			Internal::DrawFullscreen(chain.Sample, chain.Raw);

			data.Blur = glm::vec4(1.0f / width, 0.0f, depthTexel);
			SetUniformIfChanged(chain.BlurAcross, data);
			Internal::DrawFullscreen(chain.BlurAcross, chain.Blurred);

			data.Blur = glm::vec4(0.0f, 1.0f / height, depthTexel);
			SetUniformIfChanged(chain.BlurDown, data);
			Internal::DrawFullscreen(chain.BlurDown, chain.Raw);

			Internal::DrawFullscreen(chain.Apply, target.Target);
			Renderer::EndGpuTimer();
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
			if (target.AmbientOcclusion)
				bytes += 2ull * target.AmbientOcclusion->Raw->GetWidth() * target.AmbientOcclusion->Raw->GetHeight() * GetBytesPerPixel(TextureFormat::R8);
			if (target.DepthCopy)
				bytes += static_cast<uint64_t>(target.DepthCopy->GetWidth()) * target.DepthCopy->GetHeight() * GetBytesPerPixel(TextureFormat::R32F);
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
					ResizeAmbientOcclusion(target);
					if (target.DepthCopy)
						target.DepthCopy->Resize(width, height);
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
		m_Data->AmbientOcclusionSampleShader.reset();
		m_Data->AmbientOcclusionBlurShader.reset();
		m_Data->AmbientOcclusionApplyShader.reset();
		m_Data->DepthCopyShader.reset();
		DestroyAndDelete(m_Data->Black);
		m_Data->Active = false;
	}

	void PostProcessStack::Begin(const PostProcessSettings& settings, const glm::mat4& projection)
	{
		Begin(settings);
		if (m_Data->Active)
		{
			m_Data->Projection = projection;
			m_Data->HasProjection = true;
		}
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
		data.HasProjection = false;
		data.AmbientOcclusionApplied = false;
		data.DepthCopied = false;

		if (data.StatsFrame != frame)
		{
			data.StatsFrame = frame;
			data.Stats.Scenes = 0;
			data.Stats.BloomScenes = 0;
			data.Stats.AmbientOcclusionScenes = 0;
		}
		++data.Stats.Scenes;
		data.Stats.Width = width;
		data.Stats.Height = height;

		Renderer::SetRenderTarget(data.Current);
	}

	void PostProcessStack::ApplyAmbientOcclusion()
	{
		Data& data = *m_Data;
		if (!data.Active || data.AmbientOcclusionApplied || !data.Settings.AmbientOcclusion.Enabled)
			return;
		data.AmbientOcclusionApplied = true;

		if (!data.HasProjection)
		{
			if (!data.NoProjectionWarned)
			{
				DE_CORE_WARN("PostProcessStack: ambient occlusion needs the camera's projection (Begin(settings, projection)); it is skipped.");
				data.NoProjectionWarned = true;
			}
			return;
		}

		// The passes sample the scene depth, so the scene target can't stay bound while they draw.
		Framebuffer* current = Renderer::GetRenderTarget();
		data.DrawAmbientOcclusion(*data.CurrentTarget, data.Settings.AmbientOcclusion, data.Projection);
		Renderer::SetRenderTarget(current);
		++data.Stats.AmbientOcclusionScenes;
	}

	Texture* PostProcessStack::CopySceneDepth()
	{
		Data& data = *m_Data;
		if (!data.Active)
			return nullptr;

		Data::SceneTarget& target = *data.CurrentTarget;
		if (!target.DepthCopy)
		{
			if (!data.DepthCopyShader)
				data.DepthCopyShader = std::make_unique<Internal::FullscreenShader>("PostDepthCopy", "PostDepthCopy.glsl");
			target.DepthCopy = Framebuffer::Create(FramebufferParams()
				.SetDebugName("Post scene depth copy")
				.SetWidth(static_cast<int32_t>(target.Target->GetWidth()))
				.SetHeight(static_cast<int32_t>(target.Target->GetHeight()))
				.AddAttachment({ TextureFormat::R32F }));
			target.DepthCopyMaterial = data.DepthCopyShader->CreateMaterial("Post depth copy");
			target.DepthCopyMaterial->SetTexture(0, target.Target->GetDepthAttachment());
			target.DepthCopyMaterial->SetSampler(0, Renderer::GetPointSampler());
		}

		if (!data.DepthCopied)
		{
			Framebuffer* current = Renderer::GetRenderTarget();
			Internal::DrawFullscreen(target.DepthCopyMaterial, target.DepthCopy);
			Renderer::SetRenderTarget(current);
			data.DepthCopied = true;
		}
		return target.DepthCopy->GetAttachment(0);
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

		ApplyAmbientOcclusion();
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
