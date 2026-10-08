#pragma once
#include "Tests/GraphicsTest.h"
#include "Tests/TestChecks.h"

#include <memory>
#include <string>
#include <vector>

namespace Dingo
{

	// Cascaded sun shadows (Renderer3DShadowSettings). Start a mode with --shadow=sun (a box, pillars
	// receding to 40 m and a sphere on a floor), acne (a plane the sun grazes at 80 degrees from its
	// normal) or skinned (the Fox walking); --shadow-cascades tints by cascade and --shadow-pan pans the
	// camera slowly, for shimmer. The panel switches the shadows, the cascades, the biases and the
	// debug views.
	//
	// On start it checks by readback, each scene drawn twice, with the sun casting and without: the
	// floor behind a box and behind a pillar 35 m off (a far cascade) goes dark, the floor in the sun
	// is unchanged to the byte, a ShadowsOnly box through the ECS shadows the floor without being
	// drawn, the grazed plane doesn't shadow itself, and the Fox casts onto the floor.
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
			Skinned
		};

		void Check(bool condition, const std::string& name) { m_Checks.Check(condition, name); }

		void DrawMode(Renderer3D& renderer, Mode mode, bool shadows, const std::vector<glm::mat4>* palette) const;
		void DrawInto(Framebuffer* target, Mode mode, bool shadows, const PerspectiveCamera& camera);
		PerspectiveCamera CameraFor(Mode mode, float aspect) const;
		void BuildEntityScene();
		void LoadFox();
		void RunChecks();

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
		Framebuffer* m_EntityTarget = nullptr;
	};

}
