#pragma once
#include "Tests/GraphicsTest.h"
#include "Tests/TestChecks.h"

#include <array>
#include <memory>
#include <string>

namespace Dingo
{

	// The post chain (PostProcessStack) on a fixed scene of pillars under lights bright enough to clip,
	// and an emissive lamp. The viewport shows the scene through the chain with the operator, exposure,
	// knee and white point from the panel; under it, strips show each operator applied to the same HDR
	// gradient from 0 to 8. Start with --post=tonemap (the default), --post=bloom (bloom on too) or
	// --post=ao (a room with a box on the floor and one floating before the wall, with ambient
	// occlusion), --tonemap=none|soft|aces|neutral, --exposure=<EV> or --post-off.
	//
	// On start it checks by readback: every curve rises left to right, Soft is the identity up to its
	// knee and reaches 1 at its white point, None clips at 1, Soft keeps an overbright gradient's hue,
	// the scene through the chain with None comes within 1/255 of the scene without it, and a Begin with
	// the chain disabled draws exactly what drawing without it does. Then bloom: a gradient that stays
	// inside 0..1 comes out the same with bloom as without, and a small square at 8 glows past its
	// edge with bloom and not without. Then ambient occlusion, in the room: the open floor is unchanged,
	// the crease where floor meets wall darkens, and the wall beside the floating box's silhouette
	// doesn't (no halo).
	class PostTest : public GraphicsTest
	{
	public:
		PostTest() = default;
		virtual ~PostTest() = default;

	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void Cleanup() override;
		void Resize(uint32_t width, uint32_t height) override;
		void ImGuiRender() override;

	private:
		void Check(bool condition, const std::string& name) { m_Checks.Check(condition, name); }

		void DrawScene(Renderer3D& renderer) const;
		void DrawSceneInto(Framebuffer* target, const PostProcessSettings& settings);
		void DrawGradient(Framebuffer* target, const glm::vec3& base, float max, const PostProcessSettings& settings);
		void DrawSpot(Framebuffer* target, const PostProcessSettings& settings);
		void DrawRoom(Renderer3D& renderer) const;
		void DrawRoomInto(Framebuffer* target, const PostProcessSettings& settings);
		PerspectiveCamera RoomCamera(float aspect) const;
		void RunChecks();

	private:
		TestChecks m_Checks;
		bool m_ChecksDone = false;
		std::shared_ptr<int> m_Alive;

		PerspectiveCamera m_Camera;
		PostProcessSettings m_Settings;
		Material* m_LampMaterial = nullptr;

		static constexpr uint32_t k_StripWidth = 256;
		static constexpr float k_GradientMax = 8.0f;
		static constexpr int k_OperatorCount = 4;
		std::array<Framebuffer*, k_OperatorCount> m_Strips = {};
		Framebuffer* m_HueStrip = nullptr;
		Shader* m_GradientShader = nullptr;
		Material* m_GradientMaterial = nullptr;

		// Bloom: a 0..1 gradient with and without it, and a bright square with and without it.
		Framebuffer* m_FlatBloom = nullptr;
		Framebuffer* m_FlatPlain = nullptr;
		Framebuffer* m_SpotBloom = nullptr;
		Framebuffer* m_SpotPlain = nullptr;
		Shader* m_SpotShader = nullptr;
		Material* m_SpotMaterial = nullptr;

		// Three copies of the scene for the regression checks: without the chain, through it with None,
		// and through a Begin whose settings have it disabled.
		Framebuffer* m_SceneOff = nullptr;
		Framebuffer* m_SceneNone = nullptr;
		Framebuffer* m_SceneDisabled = nullptr;
		std::vector<uint8_t> m_SceneOffPixels;
		std::vector<uint8_t> m_FlatPlainPixels;

		// Ambient occlusion: the room through the chain (None) without it and with it.
		bool m_ShowRoom = false;
		Framebuffer* m_RoomPlain = nullptr;
		Framebuffer* m_RoomOccluded = nullptr;
		Framebuffer* m_SplitTarget = nullptr;
		Framebuffer* m_DepthTarget = nullptr; // k_SplitSize square, with a depth the chain must write
		std::vector<uint8_t> m_RoomPlainPixels;
	};

}
