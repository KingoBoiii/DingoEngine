#pragma once
#include "Tests/GraphicsTest.h"
#include "Tests/TestChecks.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace Dingo
{

	// Raymarch FogVolumes (FogVolume::Mode = Raymarch): the per-pixel march that runs inside the post-process
	// chain. The checks run one a frame after 10 warm-up frames. Each draws 64 x 36 RGBA16F scenes through
	// PostProcessStack (tone operator None), once without the volume and once with it, and compares the two
	// pictures pixel by pixel against a CPU model of the haze: a camera at (0, 1, 0) looks down -Z over a wall
	// whose front face is at z = -11.75, and a hazy box of uniform density covers the view from z = -2 to -10
	// unless said, so a ray's in-scatter is ambient x albedo x (1 - T), T = exp(-density x length), and the
	// pixel is the surface's colour c x T + that.
	//  1. without the post chain, or with Capabilities.Volumetrics off, nothing is marched (the volume is
	//     drawn as a froxel one, or dropped); with it, one dispatch and no froxel passes;
	//  2. a uniform ambient box over the wall at half and at quarter resolution;
	//  3. a wall inside the box: the march ends at the surface;
	//  4. the sky behind the box: black with only the in-scatter;
	//  5. a foreground box in the haze: no halo of the wall's haze on the box, or the box's on the wall, at its
	//     silhouette, at half and quarter resolution;
	//  6. a sun shaft: sunlit rays carry the sun's in-scatter, rays under a shadow caster only the ambient;
	//  7. MaxInScatter scales the strongest channel down to it and leaves the transmittance alone;
	//  8. near and far fades thin the in-scatter and leave the transmittance alone;
	//  9. 8 and 64 steps agree within 1/255;
	//  10. a global froxel fog and a raymarch box: (c x Tf + Lf) x Tr + Tf(entry) x Lr;
	//  11. a translucent quad over the box takes the haze behind it but is not hazed itself;
	//  12. an opaque custom shader is hazed as a lit surface is;
	//  13. two still frames of one scene are identical;
	//  14. each render target keeps its own half-size textures from frame to frame, whatever the size;
	//  15. FogVolumeComponents through Scene::SubmitLights draw what SubmitFogVolume draws;
	//  16. two scenes in one Begin: the second one's march sees the first one's depth; two Begins of different
	//      sizes in one frame each keep their own textures from frame to frame and match the model;
	//  17. half <-> quarter at run time keeps the textures and still matches the model;
	//  18. the shadow-sample debug view paints shadowed rays red, lit ones green and a sun without shadows yellow,
	//      and a requested froxel slice is a texture of the grid's x and y size.
	// The window shows the same effect: a wall behind a haze box, with sun shafts through a slatted roof.
	class RaymarchFogTest : public GraphicsTest
	{
	public:
		RaymarchFogTest() = default;
		virtual ~RaymarchFogTest() = default;

	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void Cleanup() override;
		void Resize(uint32_t width, uint32_t height) override;
		void ImGuiRender() override;

	public:
		enum class SolidKind
		{
			Opaque,
			Custom,
			Glass
		};

		struct Solid
		{
			SolidKind Kind = SolidKind::Opaque;
			glm::vec3 Center{ 0.0f };
			glm::vec3 Half{ 0.5f };
			glm::vec4 Color{ 1.0f };
		};

		struct Placed
		{
			FogVolume Settings;
			glm::vec3 Center{ 0.0f };
		};

		struct Sun
		{
			glm::vec3 Direction{ 0.0f, -1.0f, 0.0f };
			glm::vec3 Color{ 1.0f };
			bool Casts = false;
		};

		struct Frame
		{
			float Ambient = 0.5f;
			std::vector<Solid> Solids;
			std::vector<Sun> Suns;
			std::vector<Placed> Volumes;
			bool Global = false;
			VolumetricFog Fog;
			std::function<void(Renderer3D&)> Extra;
		};

	private:
		void Check(bool condition, const std::string& name) { m_Checks.Check(condition, name); }
		void BuildSteps();
		void CheckModes();
		void CheckUniform();
		void CheckInsideAndSky();
		void CheckDepthEdge();
		void CheckShaft();
		void CheckCapAndFades();
		void CheckSteps();
		void CheckFroxelMix();
		void CheckTranslucent();
		void CheckCustom();
		void CheckStillAndScene();
		void CheckResize();
		void CheckResolution();
		void CheckTwoScenes();
		void CheckTwoBegins();
		void CheckDebugView();

		Renderer3D::Statistics Draw(Renderer3D& renderer, const Frame& frame, Framebuffer* target, bool post = true);
		void Submit(Renderer3D& renderer, const Frame& frame);
		void DrawView();

	private:
		TestChecks m_Checks;
		bool m_Started = false;
		bool m_ChecksDone = false;
		uint32_t m_FramesBeforeChecks = 0;
		std::vector<std::function<void()>> m_Steps;
		size_t m_Step = 0;

		Renderer3D* m_Renderer = nullptr;
		Renderer3D* m_Off = nullptr;
		Renderer3D* m_View = nullptr;
		Framebuffer* m_Target = nullptr;
		Framebuffer* m_Large = nullptr;
		Mesh* m_Box = nullptr;
		Mesh* m_Quad = nullptr;
		Material* m_Alpha = nullptr;
		Shader* m_CustomShader = nullptr;
		Material* m_Custom = nullptr;
		std::shared_ptr<int> m_Alive;
		float m_Time = 0.0f;
	};

}
