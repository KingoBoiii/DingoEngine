#pragma once
#include "Tests/GraphicsTest.h"
#include "Tests/TestChecks.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace Dingo
{

	// Shadows (Renderer3DShadowSettings). Start a mode with --shadow=sun (a box, pillars receding to
	// 40 m and a sphere on a floor), acne (a plane the sun grazes at 80 degrees from its normal),
	// skinned (the Fox walking), spot (a spot light past a box), point (a point light among four
	// pillars), budget (twelve casting spot lights, four more than the shadow slots) or probe (the sun
	// scene with a row of shadow probes across the box's shadow edge, drawn as spheres from red, in
	// shadow, to green);
	// --shadow-cascades tints by cascade and --shadow-pan pans the camera slowly, for shimmer. The panel
	// switches the shadows, the cascades, the biases and the debug views.
	//
	// On start it checks by readback, each scene drawn twice, with its lights casting and without: the
	// floor behind a box and behind a pillar 35 m off (a far cascade) goes dark, the floor in the sun
	// is unchanged to the byte, a ShadowsOnly box through the ECS shadows the floor without being
	// drawn, the grazed plane doesn't shadow itself while a block on it casts, and the Fox casts onto
	// the floor; a spot light's
	// box and each of a point light's four pillars cast, the floor where nothing stands is unchanged
	// on every cube face and across their seams, eight of twelve lights get a shadow and two
	// consecutive frames of that scene are identical; and the budget fade dims the last light inside
	// the budget and lightens the shadow at the shadow slots' edge, read back. Over the next
	// frames it checks shadow probes: 0 behind an occluder (the sun's box, the spot light's box, a point
	// light's pillar, the ECS scene's ShadowsOnly box through Scene::GetLightVisibility), 1 in the
	// open and at once for a light without a shadow, and in between at the sun's PCF edge.
	class ShadowTest : public GraphicsTest
	{
	public:
		ShadowTest() = default;
		virtual ~ShadowTest() = default;

	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void Cleanup() override;
		void Resize(uint32_t width, uint32_t height) override;
		void ImGuiRender() override;

	private:
		enum class Mode : int
		{
			Sun,
			Acne,
			Skinned,
			Spot,
			Point,
			Budget,
			Probe
		};

		void Check(bool condition, const std::string& name) { m_Checks.Check(condition, name); }

		void DrawMode(Renderer3D& renderer, Mode mode, bool shadows, const std::vector<glm::mat4>* palette, bool probes = false) const;
		void DrawInto(Framebuffer* target, Mode mode, bool shadows, const PerspectiveCamera& camera, bool probes = false);
		PerspectiveCamera CameraFor(Mode mode, float aspect) const;
		void BuildEntityScene();
		void LoadFox();
		void RunChecks();
		void RunLocalChecks();
		void UpdateProbeChecks();
		void DrawBudgetSecondFrame();
		void CheckFadePixels();

	private:
		TestChecks m_Checks;
		bool m_ChecksDone = false;
		std::shared_ptr<int> m_Alive;

		Mode m_Mode = Mode::Sun;
		bool m_Shadows = true;
		bool m_Pan = false;
		float m_Time = 0.0f;
		PerspectiveCamera m_Camera;
		Renderer3DShadowSettings m_DefaultSettings;

		Model* m_Fox = nullptr;
		Material* m_FoxMaterial = nullptr;
		glm::mat4 m_FoxTransform{ 1.0f };
		glm::vec3 m_FoxMin{ 0.0f };
		glm::vec3 m_FoxMax{ 0.0f };
		Animator m_FoxAnimator;
		std::vector<glm::mat4> m_FoxPalette;

		Scene* m_EntityScene = nullptr;
		Entity m_EntitySun;

		// Frames the probe checks still issue probes for before they read the answers.
		int m_ProbeFramesLeft = 0;
		int m_ProbeAnswerFrame = -1;
		std::array<Framebuffer*, 3> m_ProbeTargets{};

		static constexpr uint32_t k_CheckWidth = 320;
		static constexpr uint32_t k_CheckHeight = 240;
		struct CheckPair
		{
			Framebuffer* On = nullptr;
			Framebuffer* Off = nullptr;
			std::vector<uint8_t> OffPixels;
		};
		CheckPair m_SunPair;
		CheckPair m_AcnePair;
		CheckPair m_FoxPair;
		CheckPair m_SpotPair;
		CheckPair m_PointPair;
		CheckPair m_BudgetPair; // the same casting scene twice: On and Off are both on
		bool m_BudgetSecondFramePending = false; // Off is drawn a frame after On

		// The budget fade's two frames, hard cut and band, read back, and where they are compared.
		std::vector<uint8_t> m_FadeHardPixels;
		std::vector<uint8_t> m_FadeBandPixels;
		glm::ivec2 m_FadeLastLitPixel{ 0 };
		glm::ivec2 m_FadeShadowPixel{ 0 };
		bool m_FadePixelsPending = false;
		Framebuffer* m_EntityTarget = nullptr;
	};

}
