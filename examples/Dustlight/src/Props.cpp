#include "Props.h"

#include "Lantern.h"
#include "RoomBuilder.h"

#include <glm/gtc/quaternion.hpp>

namespace
{
	using namespace Dingo;

	constexpr int k_Columns = 6;
	constexpr int k_Rows = 4;
	constexpr float k_SphereRadius = 0.2f;
	constexpr float k_ColumnSpacing = 0.6f;
	constexpr float k_RowSpacing = 0.45f;
	constexpr float k_FirstRowHeight = 0.62f;
	constexpr float k_GridCentreX = -0.3f;
	constexpr glm::vec3 k_EmissiveColor = { 0.35f, 0.75f, 1.0f };
	constexpr float k_RoughnessColumns[k_Columns] = { 0.05f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f };
	constexpr float k_MetallicRows[k_Rows] = { 0.0f, 0.33f, 0.66f, 1.0f };

	constexpr glm::vec3 k_TablePosition = { Lantern::k_Position.x, 0.0f, Lantern::k_Position.z };
	constexpr float k_TableScale = 1.3f;
	constexpr float k_TableTop = 0.55f * k_TableScale;
}

namespace Dingo
{

	void Props::Place(Scene& scene, const char* path, const char* name, const glm::vec3& position, float scale, float yawDegrees)
	{
		AssetManager& assets = Application::Get().GetAssetManager();
		const Model* model = assets.GetModel(assets.Load(path, ModelLoadParams().SetPbrMaterials(true)));
		if (!model)
		{
			DE_ERROR("Dustlight: failed to load {}", path);
			return;
		}

		for (const SubMesh& submesh : model->GetSubMeshes())
		{
			Entity part = scene.CreateEntity(name);
			Transform3DComponent& transform = part.AddComponent<Transform3DComponent>();
			transform.Position = position;
			transform.Scale = glm::vec3(scale);
			transform.Rotation = glm::angleAxis(glm::radians(yawDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
			part.AddComponent<MeshRendererComponent>(MeshRendererComponent(submesh.Mesh)).Material = submesh.Material;
		}
	}

	void Props::Build(Scene& scene)
	{
		Release();

		Place(scene, "models/WoodenTable_01/WoodenTable_01.gltf", "Table", k_TablePosition, k_TableScale, 0.0f);
		Place(scene, "models/brass_vase_01/brass_vase_01.gltf", "Vase", { k_TablePosition.x - 0.35f, k_TableTop, k_TablePosition.z + 0.05f }, 0.7f, 25.0f);
		Place(scene, "models/wooden_crate_01/wooden_crate_01.gltf", "Crate", { -3.0f, 0.0f, -3.0f }, 1.6f, 20.0f);
		Place(scene, "models/wooden_crate_01/wooden_crate_01.gltf", "Crate", { -1.9f, 0.0f, -3.4f }, 1.6f, -15.0f);
		Place(scene, "models/wooden_crate_01/wooden_crate_01.gltf", "Crate", { -2.7f, 0.5f, -3.1f }, 1.2f, 50.0f);

		BuildSphereGrid(scene);
	}

	void Props::BuildSphereGrid(Scene& scene)
	{
		Renderer3D& renderer = Application::Get().GetRenderer3D();
		m_Sphere = Mesh::CreateSphere(k_SphereRadius, 32, 48);

		const float zCentre = RoomLayout::k_PlinthCentre.z;
		const float firstX = k_GridCentreX + 0.5f * k_ColumnSpacing * static_cast<float>(k_Columns - 1);

		for (int row = 0; row < k_Rows; ++row)
		{
			for (int column = 0; column < k_Columns; ++column)
			{
				Material* material = renderer.CreatePbrMaterial(PbrMaterialParams()
					.SetBaseColor({ 0.95f, 0.72f, 0.35f, 1.0f })
					.SetMetallic(k_MetallicRows[row])
					.SetRoughness(k_RoughnessColumns[column]));
				m_Materials.push_back(material);

				Entity sphere = scene.CreateEntity("GridSphere");
				sphere.AddComponent<Transform3DComponent>().Position = {
					firstX - k_ColumnSpacing * static_cast<float>(column),
					k_FirstRowHeight + k_RowSpacing * static_cast<float>(row),
					zCentre };
				sphere.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_Sphere)).Material = material;
				m_GridEntities.push_back(sphere);
			}
		}

		Material* emissive = renderer.CreatePbrMaterial(PbrMaterialParams()
			.SetBaseColor({ 0.05f, 0.05f, 0.05f, 1.0f })
			.SetRoughness(0.4f)
			.SetEmissive(k_EmissiveColor)
			.SetEmissiveStrength(6.0f));
		m_Materials.push_back(emissive);

		m_EmissiveSphere = scene.CreateEntity("EmissiveSphere");
		m_EmissiveSphere.AddComponent<Transform3DComponent>().Position = { firstX - k_ColumnSpacing * static_cast<float>(k_Columns) - 0.1f, k_FirstRowHeight + 0.5f * k_RowSpacing * static_cast<float>(k_Rows - 1), zCentre };
		m_EmissiveSphere.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_Sphere)).Material = emissive;
	}

	void Props::Release()
	{
		m_GridEntities.clear();
		m_EmissiveSphere = Entity();
		delete m_Sphere;
		m_Sphere = nullptr;
		for (Material* material : m_Materials)
			delete material;
		m_Materials.clear();
	}

}
