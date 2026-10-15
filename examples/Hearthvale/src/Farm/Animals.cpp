#include "Farm/Animals.h"
#include "Life/PlayerMath.h"
#include "Sim/Fnv.h"

#include <algorithm>
#include <cmath>

namespace Dingo
{

	int Herd::CountAt(uint32_t home) const
	{
		int count = 0;
		for (const Animal& animal : m_Animals)
			count += animal.Home == home ? 1 : 0;
		return count;
	}

	const Animal* Herd::Find(uint32_t id) const
	{
		for (const Animal& animal : m_Animals)
		{
			if (animal.Id == id)
				return &animal;
		}
		return nullptr;
	}

	Animal* Herd::Find(uint32_t id)
	{
		return const_cast<Animal*>(static_cast<const Herd*>(this)->Find(id));
	}

	uint32_t Herd::Add(AnimalKind kind, uint32_t home, int happiness, bool fed)
	{
		Animal animal;
		animal.Id = m_NextId++;
		animal.Kind = kind;
		animal.Home = home;
		animal.Happiness = happiness;
		animal.Fed = fed;
		m_Animals.push_back(animal);
		return animal.Id;
	}

	void Herd::Clear()
	{
		m_Animals.clear();
		m_NextId = 1;
	}

	uint64_t Herd::Hash() const
	{
		Fnv1a hash;
		hash.U32(m_NextId);
		hash.U32(static_cast<uint32_t>(m_Animals.size()));
		for (const Animal& animal : m_Animals)
		{
			hash.U32(animal.Id);
			hash.Byte(static_cast<uint8_t>(animal.Kind));
			hash.U32(animal.Home);
			hash.I32(animal.Happiness);
			hash.Byte(animal.Fed ? 1 : 0);
			hash.Byte(animal.ProduceReady ? 1 : 0);
		}
		return hash.Value();
	}

	bool IsAnimalPen(const Occupant& occupant)
	{
		return occupant.Kind == OccupantKind::Building
			&& (occupant.DefId == static_cast<uint16_t>(BuildId::Coop) || occupant.DefId == static_cast<uint16_t>(BuildId::Barn));
	}

	AnimalKind PenKind(const Occupant& occupant)
	{
		return occupant.DefId == static_cast<uint16_t>(BuildId::Barn) ? AnimalKind::Cow : AnimalKind::Chicken;
	}

	int PenCapacity(const Occupant& occupant)
	{
		return PenKind(occupant) == AnimalKind::Cow ? BARN_CAPACITY : COOP_CAPACITY;
	}

	ItemId ProduceItem(AnimalKind kind)
	{
		return kind == AnimalKind::Cow ? ItemId::Milk : ItemId::Egg;
	}

	const char* AnimalName(AnimalKind kind)
	{
		return kind == AnimalKind::Cow ? "cow" : "chicken";
	}

	AnimalRollReport RollAnimalsDay(const TileGrid& grid, Herd& herd, bool enabled)
	{
		AnimalRollReport report;
		if (!enabled)
			return report;

		std::vector<Animal>& animals = herd.Edit();
		const size_t before = animals.size();
		std::erase_if(animals, [&grid](const Animal& animal)
		{
			const Occupant* home = grid.FindOccupant(animal.Home);
			return !home || !IsAnimalPen(*home);
		});
		report.Left = static_cast<int>(before - animals.size());

		for (Animal& animal : animals)
		{
			if (animal.Fed)
			{
				animal.Happiness = (std::min)(animal.Happiness + ANIMAL_HAPPINESS_FED, ANIMAL_HAPPINESS_MAX);
				if (animal.Happiness >= ANIMAL_HAPPINESS_PRODUCE && !animal.ProduceReady)
				{
					animal.ProduceReady = true;
					++report.ProduceMade;
				}
			}
			else
			{
				animal.Happiness = (std::max)(animal.Happiness - ANIMAL_HAPPINESS_HUNGRY, 0);
				++report.Hungry;
			}
			animal.Fed = false;
		}

		for (const Occupant& occupant : grid.GetOccupants())
		{
			if (!IsAnimalPen(occupant) || herd.CountAt(occupant.Id) >= PenCapacity(occupant))
				continue;
			herd.Add(PenKind(occupant), occupant.Id, ANIMAL_HAPPINESS_START, true);
			++report.Arrived;
		}
		return report;
	}

	TendResult TendPen(const TileGrid& grid, Herd& herd, Inventory& pack, uint32_t occupantId)
	{
		TendResult result;
		const Occupant* pen = grid.FindOccupant(occupantId);
		if (!pen || !IsAnimalPen(*pen))
			return result;

		result.IsPen = true;
		result.Kind = PenKind(*pen);
		for (Animal& animal : herd.Edit())
		{
			if (animal.Home != occupantId)
				continue;
			++result.Residents;
			if (animal.ProduceReady)
			{
				if (pack.Add(ProduceItem(animal.Kind), 1) == 1)
				{
					animal.ProduceReady = false;
					++result.Collected;
				}
				else
					++result.Blocked;
			}
			if (!animal.Fed)
			{
				animal.Fed = true;
				++result.Fed;
			}
		}
		return result;
	}

	AnimalSim::AnimalSim(ValleyDirector& director, Herd& herd, bool enabled)
		: m_Herd(herd), m_Seed(director.GetSeed()), m_Enabled(enabled)
	{
		if (!m_Enabled)
			return;

		director.OnStage(SimStage::Animals, [this](ValleyDirector& d, const StepInfo& step)
		{
			Step(d.GetGrid(), step.Dt);
		});
		director.OnDayRoll(DayRollStep::Sprinklers, [this](ValleyDirector& d, DayRollReason)
		{
			m_Last = RollAnimalsDay(d.GetGrid(), m_Herd, m_Enabled);
		});
	}

	const AnimalMotion* AnimalSim::FindMotion(uint32_t id) const
	{
		for (const AnimalMotion& motion : m_Motions)
		{
			if (motion.Id == id)
				return &motion;
		}
		return nullptr;
	}

	uint64_t AnimalSim::MotionHash() const
	{
		Fnv1a hash;
		for (const AnimalMotion& motion : m_Motions)
		{
			hash.U32(motion.Id);
			hash.U32(static_cast<uint32_t>(std::lround(motion.Position.x * 1000.0f)));
			hash.U32(static_cast<uint32_t>(std::lround(motion.Position.y * 1000.0f)));
		}
		return hash.Value();
	}

	void AnimalSim::Step(const TileGrid& grid, float dt)
	{
		if (!m_Enabled)
			return;
		Sync(grid);
		for (AnimalMotion& motion : m_Motions)
			Wander(grid, motion, dt);
	}

	void AnimalSim::Sync(const TileGrid& grid)
	{
		std::erase_if(m_Motions, [this](const AnimalMotion& motion) { return !m_Herd.Find(motion.Id); });

		for (const Animal& animal : m_Herd.All())
		{
			if (FindMotion(animal.Id))
				continue;

			AnimalMotion motion{ .Id = animal.Id, .Kind = animal.Kind, .Random = Rng(HashMix(m_Seed, animal.Id)) };
			const Occupant* home = grid.FindOccupant(animal.Home);
			if (home)
			{
				const glm::vec2 anchor((static_cast<float>(home->X) + static_cast<float>(home->W) * 0.5f) * TILE_SIZE,
					(static_cast<float>(home->Y + home->H) + 0.5f) * TILE_SIZE);
				motion.Position = anchor;
				float best = 1e9f;
				for (int y = home->Y - 2; y < home->Y + home->H + 3; ++y)
				{
					for (int x = home->X - 2; x < home->X + home->W + 3; ++x)
					{
						if (!grid.InBounds(x, y) || !PlayerMath::IsWalkable(grid, x, y))
							continue;
						const glm::vec2 centre((static_cast<float>(x) + 0.5f) * TILE_SIZE, (static_cast<float>(y) + 0.5f) * TILE_SIZE);
						const float distance = glm::dot(centre - anchor, centre - anchor);
						if (distance < best && PlayerMath::IsClear(grid, centre, ANIMAL_RADIUS))
						{
							best = distance;
							motion.Position = centre;
						}
					}
				}
			}
			motion.IdleSteps = motion.Random.NextInt(ANIMAL_IDLE_STEPS_MIN, ANIMAL_IDLE_STEPS_MAX);
			motion.Yaw = static_cast<float>(motion.Random.NextInt(0, 359)) * PlayerMath::PI / 180.0f;
			m_Motions.push_back(motion);
		}
	}

	void AnimalSim::Wander(const TileGrid& grid, AnimalMotion& motion, float dt) const
	{
		motion.Moving = false;
		if (!motion.HasTarget)
		{
			if (--motion.IdleSteps > 0)
				return;
			motion.IdleSteps = motion.Random.NextInt(ANIMAL_IDLE_STEPS_MIN, ANIMAL_IDLE_STEPS_MAX);

			const Animal* animal = m_Herd.Find(motion.Id);
			const Occupant* home = animal ? grid.FindOccupant(animal->Home) : nullptr;
			if (!home)
				return;
			const int tx = motion.Random.NextInt(home->X - ANIMAL_WANDER_TILES, home->X + home->W - 1 + ANIMAL_WANDER_TILES);
			const int ty = motion.Random.NextInt(home->Y - ANIMAL_WANDER_TILES, home->Y + home->H - 1 + ANIMAL_WANDER_TILES);
			const float jitterX = static_cast<float>(motion.Random.NextInt(20, 80)) * 0.01f;
			const float jitterY = static_cast<float>(motion.Random.NextInt(20, 80)) * 0.01f;
			const glm::vec2 target((static_cast<float>(tx) + jitterX) * TILE_SIZE, (static_cast<float>(ty) + jitterY) * TILE_SIZE);
			if (!grid.InBounds(tx, ty) || !PlayerMath::IsWalkable(grid, tx, ty) || !PlayerMath::IsClear(grid, target, ANIMAL_RADIUS))
				return;
			motion.Target = target;
			motion.HasTarget = true;
		}

		const glm::vec2 toTarget = motion.Target - motion.Position;
		const float distance = glm::length(toTarget);
		const float speed = motion.Kind == AnimalKind::Cow ? COW_SPEED : CHICKEN_SPEED;
		const float stepLength = speed * dt;
		if (distance <= stepLength)
		{
			motion.Position = motion.Target;
			motion.HasTarget = false;
			return;
		}

		const glm::vec2 direction = toTarget / distance;
		const glm::vec2 delta = direction * stepLength;
		const glm::vec2 moved = PlayerMath::ResolveMove(grid, motion.Position, delta, ANIMAL_RADIUS);
		if (glm::length(moved) < stepLength * 0.5f)
		{
			motion.HasTarget = false;
			return;
		}
		motion.Position += moved;
		motion.Yaw = PlayerMath::YawOf(direction);
		motion.Moving = true;
	}

}
