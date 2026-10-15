#pragma once
#include <DingoEngine.h>

#include "Farm/Animals.h"

#include <unordered_map>

namespace Dingo
{

	// The herd in the scene: a placeholder body and head per animal, a hop while it walks and a small egg or milk
	// marker over one with produce ready. GPU side: never constructed headless, or with ANIMALS_ENABLED off.
	class AnimalWorld
	{
	public:
		explicit AnimalWorld(Scene& scene);

		// Creates and removes entities to match the herd, then moves each to where AnimalSim has it.
		void Sync(const Herd& herd, const AnimalSim& sim, float dt);

		int EntityCount() const { return static_cast<int>(m_Pieces.size()); }

	private:
		struct Piece
		{
			Entity Root;
			Entity Marker;
			AnimalKind Kind = AnimalKind::Chicken;
			float Hop = 0.0f;
			float Lift = 0.0f;
		};

		Piece Create(const Animal& animal);

	private:
		Scene& m_Scene;
		std::unordered_map<uint32_t, Piece> m_Pieces;
		float m_Time = 0.0f;
	};

}
