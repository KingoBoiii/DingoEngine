#include "Look/ArtRegistry.h"

#include <array>

namespace Dingo::Art
{
	namespace
	{
		constexpr std::array k_Entries = {
			ArtEntry{ GROUND, ArtPrimitive::Box,    glm::vec4(0.36f, 0.6f, 0.28f, 1.0f),  glm::vec3(96.0f, 0.2f, 96.0f), {} },
			ArtEntry{ FARMER, ArtPrimitive::Box,    glm::vec4(0.85f, 0.45f, 0.25f, 1.0f), glm::vec3(0.5f, 1.0f, 0.5f),   {} },
			ArtEntry{ TREE,   ArtPrimitive::Sphere, glm::vec4(0.2f, 0.45f, 0.2f, 1.0f),   glm::vec3(1.6f),              {} },
			ArtEntry{ ROCK,   ArtPrimitive::Sphere, glm::vec4(0.5f, 0.5f, 0.52f, 1.0f),   glm::vec3(0.8f, 0.6f, 0.8f),  {} },
		};
	}

	const ArtEntry* Find(std::string_view id)
	{
		for (const ArtEntry& entry : k_Entries)
		{
			if (entry.Id == id)
				return &entry;
		}
		DE_WARN("Hearthvale: no art registered for '{}'", id);
		return nullptr;
	}

	Entity Spawn(Scene& scene, std::string_view id, const std::string& name, const glm::vec3& position)
	{
		const ArtEntry* entry = Find(id);
		if (!entry || Application::Get().IsHeadless())
			return {};

		Renderer3D& renderer = Application::Get().GetRenderer3D();
		Mesh* mesh = entry->Primitive == ArtPrimitive::Sphere ? renderer.GetSphereMesh() : renderer.GetBoxMesh();

		Entity entity = scene.CreateEntity(name);
		auto& transform = entity.AddComponent<Transform3DComponent>();
		transform.Position = position;
		transform.Scale = entry->Size;
		entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(mesh, entry->Color));
		return entity;
	}

}
