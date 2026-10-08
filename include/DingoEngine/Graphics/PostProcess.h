#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>

namespace Dingo
{

	class Framebuffer;
	class Texture;

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

	// Darkens creases, corners and the ground under things, from the scene's depth: Scalable Ambient
	// Obscurance (McGuire, Mara and Luebke 2012), 12 taps a pixel and a depth-aware blur. It multiplies
	// the whole HDR colour after the opaque 3D pass, since a forward renderer can't tell ambient light
	// from direct. Needs the camera's projection: PostProcessStack::Begin's second overload (the
	// SceneRenderer passes it).
	struct AmbientOcclusionSettings
	{
		bool Enabled = false;
		// How far around a point occluders count, in world units.
		float Radius = 0.5f;
		// How dark a fully closed corner gets.
		float Intensity = 1.0f;
		// The result is raised to this power: higher keeps open surfaces lighter and darkens corners more.
		float Power = 1.5f;
		// Ignores occluders this far in front of the surface along its normal, in world units, so a
		// flat surface doesn't occlude itself.
		float Bias = 0.02f;
		// Works at half the scene's width and height: a quarter of the cost, slightly softer edges.
		bool HalfResolution = true;
	};

	struct PostProcessSettings
	{
		// Off, the 3D pass draws straight into its target, exactly as without the post chain.
		bool Enabled = false;
		ToneMapSettings Tone;
		BloomSettings Bloom;
		AmbientOcclusionSettings AmbientOcclusion;
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
		// With the camera's projection, which ambient occlusion needs to rebuild positions from depth;
		// without it AO is skipped (warned once).
		void Begin(const PostProcessSettings& settings, const glm::mat4& projection);
		void End();

		// Applies ambient occlusion to what has been drawn into the scene target so far, once per
		// Begin; End applies it if nothing has. Call it before drawing what AO must not darken, such as
		// particles or a translucent pass.
		void ApplyAmbientOcclusion();
		// The scene target's depth so far, copied into an R32F texture (once per Begin), for a pass that
		// reads depth while drawing into the scene target with that depth bound: soft particles. Null
		// while inactive.
		Texture* CopySceneDepth();

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
			uint64_t TargetBytes = 0;   // GPU memory of every cached target, bloom levels and AO targets included
			uint32_t BloomScenes = 0;   // of Scenes, the ones that bloomed
			uint32_t AmbientOcclusionScenes = 0; // of Scenes, the ones with ambient occlusion
		};
		const Statistics& GetStatistics() const;

		// Frees the cached targets and passes; Renderer::Destroy calls it.
		void Shutdown();

	private:
		struct Data;
		std::unique_ptr<Data> m_Data;
	};

}
