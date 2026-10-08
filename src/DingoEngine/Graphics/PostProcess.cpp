#include "depch.h"
#include "DingoEngine/Graphics/PostProcess.h"
#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Graphics/FullscreenPass.h"

#include <cmath>
#include <cstring>

namespace Dingo
{

	namespace
	{
		// std140, mirrored by ToneMapData in PostToneMap.glsl.
		struct ToneMapData
		{
			glm::vec4 Tone{ 0.0f }; // x = operator, y = exposure multiplier, z = knee, w = white point
		};

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
		struct SceneTarget
		{
			Framebuffer* Target = nullptr;
			Material* ToneMap = nullptr; // samples this target alone, so its pipelines are built once
			uint64_t LastFrame = 0;
		};

		// One per output size, kept while it is used: two scenes of a frame drawing into targets of
		// different sizes (a viewport and a minimap) each keep theirs instead of resizing one back and
		// forth.
		std::vector<SceneTarget> Targets;
		std::unique_ptr<Internal::FullscreenShader> ToneMapShader;

		bool Active = false;
		bool Skipped = false;
		Framebuffer* Caller = nullptr;
		Framebuffer* Current = nullptr;
		Material* CurrentToneMap = nullptr;
		PostProcessSettings Settings;

		Statistics Stats;
		uint64_t StatsFrame = 0;
		bool NestedWarned = false;

		static constexpr uint64_t k_MaxIdleFrames = 300;

		static void Release(SceneTarget& target)
		{
			DestroyAndDelete(target.ToneMap);
			DestroyAndDelete(target.Target);
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
					target.LastFrame = frame;
					return target;
				}
			}

			if (!ToneMapShader)
				ToneMapShader = std::make_unique<Internal::FullscreenShader>("PostToneMap", "PostToneMap.glsl");

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
		data.Current = target.Target;
		data.CurrentToneMap = target.ToneMap;
		data.Settings = settings;
		data.Active = true;

		if (data.StatsFrame != frame)
		{
			data.StatsFrame = frame;
			data.Stats.Scenes = 0;
		}
		++data.Stats.Scenes;
		data.Stats.Width = width;
		data.Stats.Height = height;
		data.Stats.SceneTargets = static_cast<uint32_t>(data.Targets.size());
		data.Stats.TargetBytes = 0;
		for (const Data::SceneTarget& target : data.Targets)
			data.Stats.TargetBytes += static_cast<uint64_t>(target.Target->GetWidth()) * target.Target->GetHeight() * (GetBytesPerPixel(TextureFormat::RGBA16F) + GetBytesPerPixel(TextureFormat::D32));

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

		const ToneMapSettings& tone = data.Settings.Tone;
		const float knee = std::clamp(FiniteOr(tone.Knee, 0.8f), 0.0f, 0.999f);
		ToneMapData toneData;
		toneData.Tone = glm::vec4(
			static_cast<float>(tone.Operator),
			std::exp2(std::clamp(FiniteOr(tone.Exposure, 0.0f), -20.0f, 20.0f)),
			knee,
			std::max(FiniteOr(tone.WhitePoint, 4.0f), knee + 1e-3f));

		SetUniformIfChanged(data.CurrentToneMap, toneData);
		Internal::DrawFullscreen(data.CurrentToneMap, data.Caller);

		Renderer::EndGpuTimer();
		data.Current = nullptr;
		data.CurrentToneMap = nullptr;
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
