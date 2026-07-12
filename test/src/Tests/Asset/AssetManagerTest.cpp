#include "AssetManagerTest.h"

#include <imgui.h>

namespace Dingo
{

	void AssetManagerTest::Check(bool condition, const std::string& name)
	{
		m_Checks.push_back({ name, condition });
		if (condition)
			DE_INFO("[PASS] {}", name);
		else
			DE_ERROR("[FAIL] {}", name);
	}

	void AssetManagerTest::Initialize()
	{
		Renderer2DTest::Initialize();

		AssetManager& assets = Application::Get().GetAssetManager();
		m_Checks.clear();
		m_AsyncChecksDone = false;

		m_SyncTexture = assets.Load("textures/container.jpg");
		Check(IsValidAssetHandle(m_SyncTexture), "sync Load returns a valid handle");
		Check(assets.IsReady(m_SyncTexture), "sync Load is Ready immediately");
		Check(assets.GetTexture(m_SyncTexture) != nullptr, "GetTexture returns the loaded texture");
		Check(assets.Load("textures/container.jpg") == m_SyncTexture, "re-Load dedups to the same handle");
		Check(assets.Get<Texture>(m_SyncTexture) == assets.GetTexture(m_SyncTexture), "Get<Texture> matches GetTexture");
		Check(assets.FindByPath("textures/container.jpg") == m_SyncTexture, "FindByPath resolves the handle");
		Check(assets.GetShader(m_SyncTexture) == nullptr, "typed Get of the wrong type returns nullptr");

		m_SyncShader = assets.Load("shaders/asset_test.glsl");
		Check(assets.IsReady(m_SyncShader) && assets.GetShader(m_SyncShader) != nullptr, "file-based shader loads through the manager");

		m_FailedTexture = assets.Load("textures/does_not_exist.png");
		Check(IsValidAssetHandle(m_FailedTexture), "failed Load keeps the registration");
		Check(assets.GetState(m_FailedTexture) == AssetState::Failed, "failed Load ends in State Failed");
		Check(assets.GetTexture(m_FailedTexture) == nullptr, "failed Load Get returns nullptr");

		Check(!IsValidAssetHandle(assets.Import("data/unknown.xyz")), "unknown extension refuses Import");

		m_AsyncTexture = assets.LoadAsync("textures/hd2.png");
		m_AsyncModel = assets.LoadAsync("models/Duck/Duck.gltf");
		m_AsyncFont = assets.LoadAsync("fonts/arial.ttf");
		m_AsyncClip = assets.LoadAsync("audio/beep.wav");
		Check(IsValidAssetHandle(m_AsyncTexture) && IsValidAssetHandle(m_AsyncModel)
			&& IsValidAssetHandle(m_AsyncFont) && IsValidAssetHandle(m_AsyncClip),
			"LoadAsync returns valid handles");
		Check(assets.GetPendingCount() > 0, "LoadAsync reports pending work");
	}

	void AssetManagerTest::Update(float deltaTime)
	{
		AssetManager& assets = Application::Get().GetAssetManager();

		if (!m_AsyncChecksDone && assets.GetPendingCount() == 0)
		{
			m_AsyncChecksDone = true;

			Check(assets.IsReady(m_AsyncTexture) && assets.GetTexture(m_AsyncTexture) != nullptr, "async texture Ready (worker thread)");
			Check(assets.IsReady(m_AsyncModel) && assets.GetModel(m_AsyncModel) != nullptr, "async model Ready (main-thread fallback)");
			Check(assets.IsReady(m_AsyncFont) && assets.GetFont(m_AsyncFont) != nullptr && assets.GetFont(m_AsyncFont)->IsValid(), "async font Ready (main-thread fallback)");
			Check(assets.IsReady(m_AsyncClip) && assets.GetAudioClip(m_AsyncClip) != nullptr, "async audio clip Ready (worker thread)");

			uint32_t failed = 0;
			for (const CheckResult& check : m_Checks)
			{
				if (!check.Passed)
					failed++;
			}
			if (failed == 0)
			{
				DE_INFO("AssetManagerTest: all {} checks passed.", m_Checks.size());
				// Audible confirmation that the worker-decoded clip actually plays.
				Application::Get().GetAudioEngine().PlayOneShot(assets.GetAudioClip(m_AsyncClip), 0.5f);
			}
			else
			{
				DE_ERROR("AssetManagerTest: {} of {} checks FAILED.", failed, m_Checks.size());
			}
		}

		m_Renderer->BeginScene(m_ProjectionViewMatrix);
		m_Renderer->Clear(m_ClearColor);

		if (Texture* syncTexture = assets.GetTexture(m_SyncTexture))
			m_Renderer->DrawQuad({ -1.55f, 0.0f }, { 2.0f, 2.0f }, syncTexture);

		if (Texture* asyncTexture = assets.GetTexture(m_AsyncTexture))
			m_Renderer->DrawQuad({ 1.55f, 0.0f }, { 2.0f, 2.0f }, asyncTexture);
		else
			m_Renderer->DrawQuad({ 1.55f, 0.0f }, { 2.0f, 2.0f }, glm::vec4(0.35f, 0.35f, 0.35f, 1.0f));

		m_Renderer->EndScene();
	}

	void AssetManagerTest::Cleanup()
	{
		AssetManager& assets = Application::Get().GetAssetManager();
		assets.Remove(m_SyncTexture);
		assets.Remove(m_SyncShader);
		assets.Remove(m_FailedTexture);
		assets.Remove(m_AsyncTexture);
		assets.Remove(m_AsyncModel);
		assets.Remove(m_AsyncFont);
		assets.Remove(m_AsyncClip);

		Renderer2DTest::Cleanup();
	}

	void AssetManagerTest::ImGuiRender()
	{
		Renderer2DTest::ImGuiRender();

		AssetManager& assets = Application::Get().GetAssetManager();

		ImGui::Separator();
		ImGui::Text("Registered: %u  Loaded: %u  Pending: %u",
			assets.GetRegisteredCount(), assets.GetLoadedCount(), assets.GetPendingCount());

		if (ImGui::Button("Reload async texture"))
			assets.Reload(m_AsyncTexture);
		ImGui::SameLine();
		if (ImGui::Button("Unload async texture"))
			assets.Unload(m_AsyncTexture);
		ImGui::SameLine();
		if (ImGui::Button("LoadAsync again"))
		{
			assets.LoadAsync("textures/hd2.png");
		}

		ImGui::Separator();
		for (const CheckResult& check : m_Checks)
		{
			const ImVec4 color = check.Passed ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f) : ImVec4(0.95f, 0.3f, 0.3f, 1.0f);
			ImGui::TextColored(color, "%s %s", check.Passed ? "[PASS]" : "[FAIL]", check.Name.c_str());
		}
		if (!m_AsyncChecksDone)
			ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.3f, 1.0f), "[....] async loads in flight");
	}

}
