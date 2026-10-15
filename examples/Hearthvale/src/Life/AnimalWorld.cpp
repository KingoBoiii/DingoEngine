#include "Life/AnimalWorld.h"
#include "GameTuning.h"
#include "Look/ArtRegistry.h"

#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <vector>

namespace Dingo
{

	namespace
	{
		constexpr float k_HopHeight = 0.05f;
		constexpr float k_HopRate = 14.0f;
		constexpr float k_MarkerLift = 0.35f;
	}

	AnimalWorld::AnimalWorld(Scene& scene)
		: m_Scene(scene)
	{
	}

	AnimalWorld::Piece AnimalWorld::Create(const Animal& animal)
	{
		Piece piece;
		piece.Kind = animal.Kind;

		const ArtEntry* body = Art::Find(Art::ForAnimal(animal.Kind));
		const ArtEntry* head = Art::Find(Art::ForAnimalHead(animal.Kind));
		if (!body || !head)
			return piece;

		piece.Lift = 0.5f * body->Size.y;
		piece.Root = m_Scene.CreateEntity(animal.Kind == AnimalKind::Cow ? "Cow" : "Chicken");
		piece.Root.AddComponent<Transform3DComponent>();

		const float forward = 0.5f * body->Size.z;
		const struct { std::string_view Id; const char* Name; glm::vec3 Position; } parts[] = {
			{ Art::ForAnimal(animal.Kind),     "Body", glm::vec3(0.0f) },
			{ Art::ForAnimalHead(animal.Kind), "Head", glm::vec3(0.0f, 0.3f * body->Size.y, forward) },
		};
		for (const auto& part : parts)
		{
			Entity visual = Art::Spawn(m_Scene, part.Id, part.Name, part.Position);
			if (visual)
				visual.SetParent(piece.Root, false);
		}

		piece.Marker = Art::Spawn(m_Scene, Art::ForProduce(animal.Kind), "Produce", glm::vec3(0.0f, body->Size.y * 0.5f + k_MarkerLift, 0.0f));
		if (piece.Marker)
		{
			piece.Marker.SetParent(piece.Root, false);
			piece.Marker.SetActive(false);
		}
		return piece;
	}

	void AnimalWorld::Sync(const Herd& herd, const AnimalSim& sim, float dt)
	{
		m_Time += dt;

		std::vector<uint32_t> gone;
		for (const auto& [id, piece] : m_Pieces)
		{
			if (!herd.Find(id))
				gone.push_back(id);
		}
		for (uint32_t id : gone)
		{
			Piece& piece = m_Pieces[id];
			if (piece.Root.IsValid())
				m_Scene.DestroyEntity(piece.Root);
			m_Pieces.erase(id);
		}

		for (const Animal& animal : herd.All())
		{
			const AnimalMotion* motion = sim.FindMotion(animal.Id);
			if (!motion)
				continue;

			auto found = m_Pieces.find(animal.Id);
			if (found == m_Pieces.end())
				found = m_Pieces.emplace(animal.Id, Create(animal)).first;

			Piece& piece = found->second;
			if (!piece.Root.IsValid())
				continue;

			const float hop = motion->Moving ? std::fabs(std::sin(m_Time * k_HopRate + static_cast<float>(animal.Id))) * k_HopHeight : 0.0f;
			auto& transform = piece.Root.GetComponent<Transform3DComponent>();
			transform.Position = glm::vec3(motion->Position.x, GROUND_TOP + piece.Lift + hop, motion->Position.y);
			transform.Rotation = glm::angleAxis(motion->Yaw, glm::vec3(0.0f, 1.0f, 0.0f));

			if (piece.Marker.IsValid() && piece.Marker.IsActive() != animal.ProduceReady)
				piece.Marker.SetActive(animal.ProduceReady);
		}
	}

}
