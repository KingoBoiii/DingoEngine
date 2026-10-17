#pragma once
#include "GameTuning.h"
#include "Placing/Inventory.h"
#include "Sim/Rng.h"
#include "ValleyDirector.h"
#include "World/TileGrid.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace Dingo
{

	enum class AnimalKind : uint8_t { Chicken, Cow };

	struct Animal
	{
		uint32_t Id = 0;
		AnimalKind Kind = AnimalKind::Chicken;
		uint32_t Home = k_NoOccupant;   // the coop or barn occupant it lives in
		int Happiness = ANIMAL_HAPPINESS_START;
		bool Fed = false;
		bool ProduceReady = false;
	};

	// Who lives on the farm. Only the facts that matter to saves and the digest: where an animal stands is cosmetic
	// (AnimalSim) and never hashed.
	class Herd
	{
	public:
		const std::vector<Animal>& All() const { return m_Animals; }
		std::vector<Animal>& Edit() { return m_Animals; }
		bool Empty() const { return m_Animals.empty(); }
		int Count() const { return static_cast<int>(m_Animals.size()); }
		int CountAt(uint32_t home) const;
		const Animal* Find(uint32_t id) const;
		Animal* Find(uint32_t id);

		uint32_t Add(AnimalKind kind, uint32_t home, int happiness, bool fed);
		void Clear();

		uint64_t Hash() const;

	private:
		friend struct SaveCodec;

	private:
		std::vector<Animal> m_Animals;
		uint32_t m_NextId = 1;
	};

	struct AnimalRollReport
	{
		int Arrived = 0;
		int ProduceMade = 0;
		int Hungry = 0;
		int Left = 0;
	};

	// Whether the occupant is a coop or barn, and what it holds.
	bool IsAnimalPen(const Occupant& occupant);
	AnimalKind PenKind(const Occupant& occupant);
	int PenCapacity(const Occupant& occupant);
	ItemId ProduceItem(AnimalKind kind);
	const char* AnimalName(AnimalKind kind);

	// The animals' day roll: those whose pen is gone leave, fed ones gain happiness and make produce, unfed ones lose
	// it, then each pen with room takes in one new animal. `enabled` false does nothing at all.
	AnimalRollReport RollAnimalsDay(const TileGrid& grid, Herd& herd, bool enabled);

	struct TendResult
	{
		bool IsPen = false;
		AnimalKind Kind = AnimalKind::Chicken;
		int Residents = 0;
		int Collected = 0;
		int Blocked = 0;     // produce left because the pack is full
		int Fed = 0;
	};

	// Interact on a coop or barn: take the ready produce that fits the pack, then feed everyone who is hungry.
	TendResult TendPen(const TileGrid& grid, Herd& herd, Inventory& pack, uint32_t occupantId);

	struct AnimalMotion
	{
		uint32_t Id = 0;
		AnimalKind Kind = AnimalKind::Chicken;
		glm::vec2 Position{ 0.0f };
		glm::vec2 Target{ 0.0f };
		bool HasTarget = false;
		int IdleSteps = 0;
		float Yaw = 0.0f;
		bool Moving = false;
		Rng Random;
	};

	// The wander of every animal in the herd, one SimStage::Animals step at a time, plus the day roll hook. Plain C++:
	// no renderer. Constructed disabled it registers nothing, so a valley without animals pays and stores nothing.
	class AnimalSim
	{
	public:
		AnimalSim(ValleyDirector& director, Herd& herd, bool enabled = ANIMALS_ENABLED);
		AnimalSim(const AnimalSim&) = delete;
		AnimalSim& operator=(const AnimalSim&) = delete;

		bool IsEnabled() const { return m_Enabled; }
		const std::vector<AnimalMotion>& GetMotions() const { return m_Motions; }
		const AnimalMotion* FindMotion(uint32_t id) const;
		const AnimalRollReport& GetLastRoll() const { return m_Last; }
		uint64_t MotionHash() const;

		void Step(const TileGrid& grid, float dt);
		// After a load or a new game: every animal starts again at its pen, with wander streams from `seed`.
		void Reset(uint32_t seed);

	private:
		void Sync(const TileGrid& grid);
		void Wander(const TileGrid& grid, AnimalMotion& motion, float dt) const;

	private:
		Herd& m_Herd;
		uint32_t m_Seed;
		bool m_Enabled;
		std::vector<AnimalMotion> m_Motions;
		AnimalRollReport m_Last;
	};

}
