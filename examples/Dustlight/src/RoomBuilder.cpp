#include "RoomBuilder.h"

namespace Dingo
{

	Material* RoomBuilder::MakeMaterial(const glm::vec3& color, float roughness)
	{
		Material* material = Application::Get().GetRenderer3D().CreatePbrMaterial(
			PbrMaterialParams().SetBaseColor(glm::vec4(color, 1.0f)).SetRoughness(roughness).SetMetallic(0.0f));
		m_Materials.push_back(material);
		return material;
	}

	Entity RoomBuilder::AddBox(Scene& scene, const char* name, const glm::vec3& min, const glm::vec3& max, Material* material)
	{
		Entity entity = scene.CreateEntity(name);
		Transform3DComponent& transform = entity.AddComponent<Transform3DComponent>();
		transform.Position = (min + max) * 0.5f;
		transform.Scale = max - min;
		MeshRendererComponent& renderer = entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_Box));
		renderer.Material = material;
		renderer.Static = true;
		return entity;
	}

	void RoomBuilder::Build(Scene& scene)
	{
		using namespace RoomLayout;
		Release();
		m_Box = Mesh::CreateBox(1.0f, 1.0f, 1.0f);

		Material* plaster = MakeMaterial({ 0.78f, 0.72f, 0.62f }, 0.92f);
		Material* stone = MakeMaterial({ 0.42f, 0.39f, 0.36f }, 0.72f);
		Material* terrace = MakeMaterial({ 0.74f, 0.68f, 0.58f }, 0.85f);
		Material* wood = MakeMaterial({ 0.22f, 0.13f, 0.07f }, 0.6f);
		Material* plinthPanel = MakeMaterial({ 0.08f, 0.08f, 0.09f }, 0.9f);

		constexpr float w = k_HalfWidth;
		constexpr float l = k_HalfLength;
		constexpr float t = 0.3f;

		AddBox(scene, "Floor", { -w - t, -t, -l - t }, { w + t, 0.0f, l + t }, stone);
		AddBox(scene, "Ceiling", { -w - t, k_Height, -l - t }, { w + t, k_Height + t, l + t }, plaster);
		AddBox(scene, "EastWall", { w, 0.0f, -l - t }, { w + t, k_Height, l + t }, plaster);
		AddBox(scene, "NorthWall", { -w - t, 0.0f, l }, { w + t, k_Height, l + t }, plaster);

		AddBox(scene, "WestWallSouth", { -w - t, 0.0f, -l - t }, { -w, k_Height, -2.0f }, plaster);
		AddBox(scene, "WestWallNorth", { -w - t, 0.0f, 2.0f }, { -w, k_Height, l + t }, plaster);
		AddBox(scene, "WestLintel", { -w - t, 4.0f, -2.0f }, { -w, k_Height, 2.0f }, plaster);

		constexpr float sillY = 1.0f;
		constexpr float headY = 3.6f;
		constexpr float southZ0 = -l - t;
		constexpr float southZ1 = -l;
		AddBox(scene, "SouthWallSill", { -w - t, 0.0f, southZ0 }, { w + t, sillY, southZ1 }, plaster);
		AddBox(scene, "SouthWallHead", { -w - t, headY, southZ0 }, { w + t, k_Height, southZ1 }, plaster);

		constexpr float windowCentres[] = { -3.5f, 0.0f, 3.5f };
		constexpr float windowHalf = 0.7f;
		float pillarStart = -w - t;
		for (int i = 0; i < 3; ++i)
		{
			AddBox(scene, "SouthPillar", { pillarStart, sillY, southZ0 }, { windowCentres[i] - windowHalf, headY, southZ1 }, plaster);
			pillarStart = windowCentres[i] + windowHalf;

			const float cx = windowCentres[i];
			AddBox(scene, "MullionV", { cx - 0.03f, sillY, southZ0 + 0.1f }, { cx + 0.03f, headY, southZ1 - 0.1f }, wood);
			AddBox(scene, "MullionH", { cx - windowHalf, 2.27f, southZ0 + 0.1f }, { cx + windowHalf, 2.33f, southZ1 - 0.1f }, wood);
		}
		AddBox(scene, "SouthPillar", { pillarStart, sillY, southZ0 }, { w + t, headY, southZ1 }, plaster);

		AddBox(scene, "Terrace", { -w - 4.0f, -t, southZ0 - 5.0f }, { w + 4.0f, 0.0f, southZ0 }, terrace);

		AddBox(scene, "Plinth", k_PlinthCentre - glm::vec3(4.0f, 0.2f, 0.7f), k_PlinthCentre + glm::vec3(4.0f, 0.2f, 0.7f), stone);
		AddBox(scene, "GridBackPanel", { -3.1f, k_PlinthTop, k_PlinthCentre.z + 0.45f }, { 1.8f, 2.45f, k_PlinthCentre.z + 0.55f }, plinthPanel);

		AddBox(scene, "AlcoveFloor", { -8.3f, -t, -2.3f }, { -w, 0.0f, 2.3f }, stone);
		AddBox(scene, "AlcoveBack", { -8.3f, 0.0f, -2.3f }, { -8.0f, 3.6f, 2.3f }, plaster);
		AddBox(scene, "AlcoveRailSouth", { -8.3f, 0.0f, -2.3f }, { -w, 1.0f, -2.0f }, plaster);
		AddBox(scene, "AlcoveRailNorth", { -8.3f, 0.0f, 2.0f }, { -w, 1.0f, 2.3f }, plaster);

		for (int i = 0; i < 9; ++i)
		{
			const float z = -2.0f + 0.5f * static_cast<float>(i);
			AddBox(scene, "PergolaBeam", { -8.3f, 3.3f, z - 0.07f }, { -w, 3.45f, z + 0.07f }, wood);
		}
		AddBox(scene, "PergolaRail", { -8.3f, 3.15f, -2.3f }, { -8.15f, 3.3f, 2.3f }, wood);
		AddBox(scene, "PergolaRail", { -6.3f, 3.15f, -2.3f }, { -6.15f, 3.3f, 2.3f }, wood);
	}

	void RoomBuilder::Release()
	{
		delete m_Box;
		m_Box = nullptr;
		for (Material* material : m_Materials)
			delete material;
		m_Materials.clear();
	}

}
