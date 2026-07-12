#pragma once
#include "Tests/Renderer2D/Renderer2DTest.h"

#include <string>
#include <vector>

namespace Dingo
{

	// Exercises the AssetManager end to end: sync loading + path dedup, typed access,
	// the failure contract, background loading (worker-thread texture/audio decode and
	// the main-thread fallback queue for model/font), and Reload/Unload/Remove.
	// Renders the sync-loaded texture next to the async-loaded one (placeholder quad
	// until it arrives); check results show in the Properties panel and the log.
	class AssetManagerTest : public Renderer2DTest
	{
	public:
		AssetManagerTest(Renderer2D* renderer)
			: Renderer2DTest(renderer)
		{}
		virtual ~AssetManagerTest() = default;

	public:
		virtual void Initialize() override;
		virtual void Update(float deltaTime) override;
		virtual void Cleanup() override;
		virtual void ImGuiRender() override;

	private:
		void Check(bool condition, const std::string& name);

	private:
		struct CheckResult
		{
			std::string Name;
			bool Passed;
		};
		std::vector<CheckResult> m_Checks;

		AssetHandle m_SyncTexture = k_InvalidAsset;
		AssetHandle m_SyncShader = k_InvalidAsset;
		AssetHandle m_FailedTexture = k_InvalidAsset;
		AssetHandle m_AsyncTexture = k_InvalidAsset;
		AssetHandle m_AsyncModel = k_InvalidAsset;
		AssetHandle m_AsyncFont = k_InvalidAsset;
		AssetHandle m_AsyncClip = k_InvalidAsset;

		bool m_AsyncChecksDone = false;
	};

}
