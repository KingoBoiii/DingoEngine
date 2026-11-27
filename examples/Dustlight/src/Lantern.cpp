#include "Lantern.h"

#include "RoomBuilder.h"

#include <cmath>

namespace
{
	constexpr float k_SpotIntensity = 1.5f;
	constexpr float k_GlowIntensity = 0.8f;
	constexpr float k_FlickerDepth = 0.12f;
	constexpr glm::vec3 k_FlameColor = { 1.0f, 0.62f, 0.28f };
	constexpr float k_LightHeight = 1.43f;

	float Flicker(float clock)
	{
		return 1.0f - k_FlickerDepth * std::abs(std::sin(clock * 23.0f) * std::sin(clock * 9.0f + 0.7f));
	}
}

namespace Dingo
{

	void Lantern::Release()
	{
		m_Spot = Entity();
		m_Glow = Entity();
		delete m_Chain;
		m_Chain = nullptr;
	}

	void Lantern::Build(Scene& scene)
	{
		AssetManager& assets = Application::Get().GetAssetManager();
		const AssetHandle handle = assets.Load("models/Lantern_01/Lantern_01.gltf", ModelLoadParams().SetPbrMaterials(true));
		const Model* model = assets.GetModel(handle);
		if (!model)
		{
			DE_ERROR("Dustlight: failed to load the lantern model");
		}
		else
		{
			for (const SubMesh& submesh : model->GetSubMeshes())
			{
				Entity part = scene.CreateEntity("Lantern");
				Transform3DComponent& transform = part.AddComponent<Transform3DComponent>();
				transform.Position = k_Position;
				transform.Scale = glm::vec3(k_Scale);
				MeshRendererComponent& renderer = part.AddComponent<MeshRendererComponent>(MeshRendererComponent(submesh.Mesh));
				renderer.Material = submesh.Material;
				renderer.ShadowGroups = 2;
				if (submesh.Material && submesh.Material->GetParams().Translucent)
					renderer.SetSurface(SurfaceOverride().SetEmissiveColor(k_FlameColor).SetEmissiveStrength(2.5f).SetRoughness(0.25f));
			}
		}

		m_Chain = Mesh::CreateBox(0.02f, 1.0f, 0.02f);
		const float chainBottom = k_Position.y + 0.3f * k_Scale;
		Entity chain = scene.CreateEntity("LanternChain");
		Transform3DComponent& chainTransform = chain.AddComponent<Transform3DComponent>();
		chainTransform.Position = { k_Position.x, (chainBottom + RoomLayout::k_Height) * 0.5f, k_Position.z };
		chainTransform.Scale = { 1.0f, RoomLayout::k_Height - chainBottom, 1.0f };
		chain.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_Chain, glm::vec4(0.1f, 0.1f, 0.1f, 1.0f))).ShadowGroups = 2;

		m_Spot = scene.CreateEntity("LanternSpot");
		m_Spot.AddComponent<Transform3DComponent>().Position = { k_Position.x, k_LightHeight - 0.02f, k_Position.z };
		SpotLightComponent& spot = m_Spot.AddComponent<SpotLightComponent>();
		spot.Color = k_FlameColor;
		spot.Intensity = k_SpotIntensity;
		spot.Range = 8.0f;
		spot.InnerConeAngle = 22.0f;
		spot.OuterConeAngle = 30.0f;
		spot.Direction = { 0.0f, -1.0f, 0.0f };
		spot.CastShadows = true;
		spot.ShadowCasterGroups = 1;
		spot.VolumetricIntensity = 6.0f;

		m_Glow = scene.CreateEntity("LanternGlow");
		m_Glow.AddComponent<Transform3DComponent>().Position = { k_Position.x, k_LightHeight, k_Position.z };
		PointLightComponent& glow = m_Glow.AddComponent<PointLightComponent>();
		glow.Color = k_FlameColor;
		glow.Intensity = k_GlowIntensity;
		glow.Range = 5.0f;
		glow.VolumetricIntensity = 1.0f;
		glow.ShadowCasterGroups = 1;

		m_SpotIntensity = k_SpotIntensity;
		m_GlowIntensity = k_GlowIntensity;
	}

	void Lantern::SetEnabled(bool enabled)
	{
		m_Enabled = enabled;
		if (m_Spot.IsValid())
			m_Spot.GetComponent<SpotLightComponent>().Enabled = enabled;
		if (m_Glow.IsValid())
			m_Glow.GetComponent<PointLightComponent>().Enabled = enabled;
	}

	void Lantern::Update(float clock)
	{
		const float flicker = Flicker(clock);
		if (m_Spot.IsValid())
			m_Spot.GetComponent<SpotLightComponent>().Intensity = m_SpotIntensity * m_Scale * flicker;
		if (m_Glow.IsValid())
			m_Glow.GetComponent<PointLightComponent>().Intensity = m_GlowIntensity * m_Scale * flicker;
	}

}
