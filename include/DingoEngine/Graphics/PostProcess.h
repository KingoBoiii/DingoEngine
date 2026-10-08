#pragma once

#include <cstdint>
#include <memory>

namespace Dingo
{

	class Framebuffer;

	// What happens to light past 1.0. The engine stays display-referred: textures, lights and the lit
	// shader mean what they always did, and every operator but Soft is for a game re-tuned to it.
	enum class ToneMapOperator
	{
		None,    // clips at 1.0, as without the post chain
		Soft,    // identity up to Knee, then rolls off to 1.0 at WhitePoint, on the max channel so hue holds
		ACES,    // a filmic toe and shoulder (Narkowicz's fit); shifts every value
		Neutral  // Khronos PBR Neutral: a small offset darkens the darks, then compresses near 1
	};

	struct ToneMapSettings
	{
		ToneMapOperator Operator = ToneMapOperator::Soft;
		// In stops, applied before the curve: colour x 2^Exposure.
		float Exposure = 0.0f;
		// Soft: values whose brightest channel is at most Knee pass unchanged.
		float Knee = 0.8f;
		// Soft: the brightest channel reaching this maps to exactly 1.0, and anything brighter too.
		float WhitePoint = 4.0f;
	};

	// A glow around light brighter than Threshold, added to the scene before the tone curve: the
	// dual-filter bloom of Jimenez's "Next Generation Post Processing in Call of Duty: Advanced
	// Warfare", six levels down from half resolution and back up. With the default threshold only what
	// would have clipped blooms, so a scene inside 0..1 gains nothing; lit materials' EmissiveStrength
	// past 1 is what it is for.
	struct BloomSettings
	{
		bool Enabled = false;
		// How much of the glow is added.
		float Intensity = 0.5f;
		// The brightest channel must pass this before anything blooms...
		float Threshold = 1.0f;
		// ...and the glow grows in smoothly over this much more (in the same units), so a light
		// brightening past the threshold doesn't switch its glow on.
		float Knee = 0.1f;
		// The spread of each level's upsample, in its source's texels: 1 is the paper's tent.
		float Radius = 1.0f;
	};

	struct PostProcessSettings
	{
		// Off, the 3D pass draws straight into its target, exactly as without the post chain.
		bool Enabled = false;
		ToneMapSettings Tone;
		BloomSettings Bloom;
	};

	// The 3D pass's post chain. Begin redirects the draws that follow into an HDR scene target (RGBA16F
	// colour and a sampleable depth, the size of the render target that was current); End tone-maps it
	// into that render target and makes it current again. 2D drawn afterwards goes on top untouched:
	// the HUD is never tone mapped.
	//
	//   PostProcessStack& post = Renderer::GetPostProcessStack();
	//   post.Begin(settings);
	//   renderer3D.BeginScene(camera); renderer3D.Clear(...); ...; renderer3D.EndScene();
	//   post.End();
	//
	// SceneRenderer does this for a camera entity with a PostProcessComponent. Blending in RGBA16F
	// differs from 8-bit by rounding, so ToneMapOperator::None comes within 1/255 of the frame with the
	// chain off, not exactly to it. A Begin with Enabled false, or one while the frame renders nothing,
	// changes nothing, and its End does nothing either.
	class PostProcessStack
	{
	public:
		PostProcessStack();
		~PostProcessStack();
		PostProcessStack(const PostProcessStack&) = delete;
		PostProcessStack& operator=(const PostProcessStack&) = delete;

		void Begin(const PostProcessSettings& settings);
		void End();

		// Between a Begin that took effect and its End.
		bool IsActive() const;
		// The HDR target the 3D pass draws into while active, else null.
		Framebuffer* GetSceneTarget() const;

		struct Statistics
		{
			uint32_t Scenes = 0;        // Begin/End pairs that ran the chain in the last frame that ran one
			uint32_t Width = 0;         // the last scene target's size
			uint32_t Height = 0;
			uint32_t SceneTargets = 0;  // cached, one per output size in use
			uint64_t TargetBytes = 0;   // GPU memory of every cached target, bloom levels included
			uint32_t BloomScenes = 0;   // of Scenes, the ones that bloomed
		};
		const Statistics& GetStatistics() const;

		// Frees the cached targets and passes; Renderer::Destroy calls it.
		void Shutdown();

	private:
		struct Data;
		std::unique_ptr<Data> m_Data;
	};

}
