#pragma once
#include "Tests/GraphicsTest.h"
#include "Tests/TestChecks.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Dingo
{

	// Environment and the skybox. The test writes its own 128 x 64 RGBE panorama (a smooth gradient
	// plus a sun at radiance 20) and checks, by readback unless noted:
	//  1. the baked cube's mip 0 on all six faces matches a CPU resample of the panorama through
	//     Environment.glsl and CubeMap.glsl;
	//  2. every mip of every face is the 2 x 2 average of the mip above it, down to 1 x 1;
	// 14. an Environment deleted before its first bake, one deleted in the frame that baked it, and one
	//     deleted a frame later leave no validation error (read the log);
	// 15. Create returns nullptr, with an error, for a file that is not a .hdr, a .hdr that is not a
	//     Radiance image, a missing file and a face size that is not a power of two from 16 to 2048.
	//  3. a 32 x 32 view down each of -Z, +X, -X, +Y and -Y shows the panorama's colour there;
	//  4. Rotation 90 turns the sky about +Y: looking -X shows what -Z showed, looking -Z shows +X;
	//  5. Intensity 2 doubles the linear radiance and Intensity 0 draws black;
	//  6. an opaque box and a quad near the far plane draw identically with and without the sky, and the
	//     sky fills every pixel they leave;
	//  7. a lit and a PBR sphere draw identically with and without the sky when its Lighting is off;
	//  8. through the post chain (tone map None, exposure -2 EV) the sun, over 1 in the HDR scene target,
	//     reaches the swap target as a quarter of its encoded value;
	// 10. two environments and two rotations in the scenes of one frame each draw their own sky;
	// 11. a fogged scene leaves its sky pixels unchanged;
	// 12. an orthographic camera, DrawSkybox false and an invalid intensity draw no sky.
	//  9. SkyboxComponent: the camera entity's own sky beats the scene-wide one, Enabled and Visible
	//     false, an InactiveComponent entity, another camera's sky, the first of two in entity order,
	//     SetEnvironment after SubmitLights, and a null Environment;
	// 13. an AssetManager Load of a .hdr gives an Environment, a reload keeps the pointer and cube id
	//     with the new contents, a pack round trip through a temporary .dpak, and LoadAsync.
	// The view shows four panes: the sky with a
	// lit and a PBR sphere, the six axes, the sky turned 90 degrees, and the post chain with an exposure.
	class SkyboxTest : public GraphicsTest
	{
	public:
		SkyboxTest() = default;
		virtual ~SkyboxTest() = default;

	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void Cleanup() override;
		void Resize(uint32_t width, uint32_t height) override;
		void ImGuiRender() override;

	private:
		void Check(bool condition, const std::string& name) { m_Checks.Check(condition, name); }
		void RunChecks();
		void CheckBake();
		void CheckLifetime();
		void CheckRejections();
		void CheckSky();
		void CheckScene();
		void CheckAssets();
		void DrawView();

		Framebuffer* NewTarget(const char* name, bool depth = true);
		glm::mat4 ShotMatrix(const glm::vec3& direction, float aspect = 1.0f) const;
		Renderer3D::Statistics RenderShot(Framebuffer* target, const glm::mat4& viewProjection, const SceneEnvironment* sky, const std::function<void(Renderer3D&)>& submit = {});
		void ReadTargets(std::vector<Framebuffer*> targets, std::function<void(const std::vector<TexturePixels>&)> done);
		void SubmitSpheres(Renderer3D& renderer);

	private:
		TestChecks m_Checks;
		bool m_ChecksDone = false;
		std::shared_ptr<int> m_Alive;
		std::filesystem::path m_Folder;
		std::filesystem::path m_HdrFile;
		std::vector<float> m_Source;

		Environment* m_Environment = nullptr;
		Environment* m_LateDelete = nullptr;
		Environment* m_Second = nullptr;
		AssetHandle m_AssetHandle = k_InvalidAsset;
		AssetHandle m_AsyncHandle = k_InvalidAsset;
		bool m_AsyncPending = false;
		uint32_t m_AsyncFrames = 0;
		Renderer3D* m_Renderer = nullptr;
		Material* m_PbrMaterial = nullptr;
		std::vector<Framebuffer*> m_Targets;
		uint32_t m_Width = 1280;
		uint32_t m_Height = 720;
		uint32_t m_FramesSinceChecks = 0;
	};

}
