#pragma once
#include "Farm/GameState.h"
#include "Farm/SleepFlow.h"
#include "Life/PlayerIntent.h"
#include "Life/VillagerSim.h"
#include "Placing/BuildController.h"
#include "ValleyDirector.h"

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace Dingo
{

	struct AutoplayStats
	{
		int Tilled = 0;
		int Planted = 0;
		int Watered = 0;
		int Harvested = 0;
		int Shipped = 0;
		int SeedsBought = 0;
		int Fences = 0;
		int Lamps = 0;
		int Gifts = 0;
		int Nights = 0;
	};

	// The one line a played day logs, from the finished day's state. `--days` and the determinism check both use it.
	std::string FormatDayLine(const ValleyDirector& director, const GameState& state);

	// "tilled 40 planted 88 ...": what the farmer did, for the end of a run.
	std::string FormatAutoplayStats(const AutoplayStats& stats);

	// The autoplay farmer. Each day it buys what it lacks at the shop, tills, plants, waters and harvests a field
	// of crop rows, ships the harvest, builds a fence run and a lamp, gives a board gift when it holds one and goes
	// to bed. It acts only through the sim's own entry points: the farm actions go through a PlayerIntent into
	// BuildController::Step (the same call the keyboard reaches), and the shop, the bin and a gift call the
	// functions their screens call. It needs no Player: it walks as tile hops along FindRoute at PLAYER_SPEED
	// and keeps its own feet and facing, which a windowed run copies onto the Player. Nothing in it reads a
	// clock or a random number, so a seed and a start give the same days.
	class AutoplayFarmer
	{
	public:
		AutoplayFarmer(ValleyDirector& director, GameState& state, BuildController& build, SleepFlow& sleep, const VillagerSim* villagers);

		// One sim step, in the ToolsAndPlacement stage. `inputBlocked` is a screen the farmer cannot act through.
		void Step(bool inputBlocked = false);

		// Forgets today's plan and stands at the farmhouse door, e.g. after a game was loaded.
		void Reset();

		glm::vec2 GetFeet() const { return m_Feet; }
		float GetYaw() const { return m_Yaw; }
		// The world (x, z) direction the last step walked, or zero.
		glm::vec2 GetHeading() const { return m_Heading; }
		const AutoplayStats& GetStats() const { return m_Stats; }

	private:
		enum class StopKind : uint8_t { Field, Shop, Bin, BuildRun, Fence, Lamp, Gift, Bed };
		enum class FieldNeed : uint8_t { None, Harvest, Till, Plant, Water };

		struct Stop
		{
			StopKind Kind = StopKind::Field;
			glm::ivec2 Stand{ 0 };
			glm::ivec2 Face{ 0 };
			VillagerId Who = VillagerId::Count;
			int Retries = 0;
		};

		void PlanDay(int day);
		void Act(PlayerIntent& intent);
		bool StartStop();
		void FinishStop();
		bool Walk();
		void Arrive();
		bool Work(PlayerIntent& intent);
		bool WorkField(PlayerIntent& intent);
		bool WorkPlace(PlayerIntent& intent);
		bool WorkBed(PlayerIntent& intent);
		void Shop();
		void ShipHarvest();
		bool GiveGiftNow();
		void ExpandBuildRun();
		void Face(const glm::ivec2& from, const glm::ivec2& to);

		glm::ivec2 FindFenceRun() const;
		FieldNeed NeedOf(int x, int y) const;
		CropId GiftCrop() const;
		CropId ChooseSeed() const;
		CropId BestCrop() const;
		int DaysLeftInSeason() const;
		int FreeFieldTiles() const;
		int SeedsForSeason() const;
		int FencesStanding() const;
		bool HasBuilding(BuildId id) const;
		bool WantsShop() const;
		int ShippableCount() const;
		struct Materials { int Wood = 0; int Stone = 0; };
		Materials MissingMaterials() const;
		struct SeedBuy { CropId Crop = CropId::Count; int Count = 0; };
		SeedBuy PlanSeeds() const;
		bool FindGiftRequest(VillagerId& who, ItemId& item) const;
		ItemId KeptForGift() const;

	private:
		ValleyDirector& m_Director;
		GameState& m_State;
		BuildController& m_Build;
		SleepFlow& m_Sleep;
		const VillagerSim* m_Villagers;

		glm::vec2 m_Feet{ 0.0f };
		float m_Yaw = 0.0f;
		glm::vec2 m_Heading{ 0.0f };

		int m_PlanDay = -1;
		std::deque<Stop> m_Stops;
		Stop m_Current;
		bool m_HasStop = false;
		bool m_Walking = false;
		std::vector<glm::ivec2> m_Route;
		size_t m_RouteIndex = 0;
		int m_StopSteps = 0;
		int m_Attempts = 0;
		int m_ActionGap = 0;
		int m_BedSteps = 0;
		int m_Idle = 0;
		AutoplayStats m_Stats;
		FieldNeed m_Pending = FieldNeed::None;
		glm::ivec2 m_FenceStart{ -1 };
	};

}
