#pragma once
#include "Farm/Calendar.h"
#include "Farm/Crops.h"
#include "Placing/Inventory.h"
#include "Placing/Placement.h"
#include "Placing/Targeting.h"
#include "Placing/Tools.h"
#include "World/ValleyLayout.h"
#include "Life/PlayerIntent.h"

#include <cstdint>
#include <functional>
#include <string>

namespace Dingo
{

	enum class BuildMode : uint8_t { Tools, Build };

	inline constexpr ItemId BUILD_TOOLS[] = { ItemId::Hoe, ItemId::WateringCan, ItemId::Axe, ItemId::Pickaxe, ItemId::Scythe };

	// The ToolsAndPlacement stage: what the farmer aims at, which tool or buildable is picked, and the swing, place
	// and remove actions the intent asks for. Plain C++ over the grid, so --check drives it headless. Every grid edit
	// is reported through `onDirty` with the tiles it touched; the layer forwards that to the ValleyWorld, so one edit
	// is one rebuilt cell. In tool mode the inventory's hotbar picks what the farmer holds: Use swings a held tool
	// (hoe tills, can waters, scythe harvests a ripe crop, else clears) or plants a held seed, and Interact goes to
	// bed at the farmhouse, harvests a ripe crop or plants the selected seed. A pickup that does not fit the
	// inventory is refused and leaves the target standing.
	class BuildController
	{
	public:
		using DirtyFn = std::function<void(const TileRect&)>;
		using SleepFn = std::function<void()>;
		// Called when the interact pulse lands on a landmark other than the farmhouse; true when it opened something.
		using InteractFn = std::function<bool(Landmark)>;

		BuildController(TileGrid& grid, Inventory& inventory, DirtyFn onDirty);

		// Resolves the aim, then applies the intent's selection, rotate, use and interact pulses in that order.
		void Step(const PlayerIntent& intent, const AimInput& aim);

		BuildMode GetMode() const { return m_Mode; }
		// What the selected hotbar slot holds; ItemId::Count when it is empty.
		ItemId GetHeld() const;
		// The held item when it is a tool, else ItemId::Count.
		ItemId GetTool() const;
		BuildId GetBuildable() const { return m_Buildable; }
		int GetRotation() const { return m_Rotation; }
		const Aim& GetAim() const { return m_Aim; }
		// The spec Use would place now; valid in either mode.
		PlaceSpec GetSpec() const { return m_Spec; }
		PlaceResult GetPreview() const { return m_Preview; }
		const std::string& GetMessage() const { return m_Message; }
		uint32_t GetMessageSerial() const { return m_MessageSerial; }

		void SetMode(BuildMode mode) { m_Mode = mode; }
		// Selects the hotbar slot holding `tool`; nothing changes when no hotbar slot does.
		void SelectTool(ItemId tool);
		void SelectBuildable(BuildId id) { m_Buildable = id; }
		void SetRotation(int quarterTurns) { m_Rotation = quarterTurns & 3; }

		// The calendar gives the day, season and weather the farm actions use; without one it is a clear spring day 0.
		void SetCalendar(const Calendar* calendar) { m_Calendar = calendar; }
		void SetSleepHandler(SleepFn onSleep) { m_OnSleep = std::move(onSleep); }
		void SetInteractHandler(InteractFn onInteract) { m_OnInteract = std::move(onInteract); }
		// In build mode, holding Use after a placing press places on each new tile the aim reaches.
		void SetHoldToPlace(bool enabled);
		bool GetHoldToPlace() const { return m_HoldToPlace; }
		void SelectSeed(CropId crop) { m_Seed = crop; }
		void CycleSeed();
		// The seed Plant would use: the selected one if held, else the first held seed that grows this season.
		CropId GetSeed() const;

		// The actions themselves, for the bot as well as the intent.
		PlaceOutcome Place();
		RemoveOutcome Remove();
		ToolOutcome UseTool();
		// The Use pulse in tool mode.
		void UseSelectedTool();
		FarmOutcome Till();
		FarmOutcome Water();
		FarmOutcome Harvest();
		FarmOutcome Plant();
		// The Interact pulse in tool mode; true when it did something.
		bool InteractTool();

	private:
		void Refresh(const AimInput& aim);
		void CycleSelection(int direction);
		void SyncSeedToHeld();
		void Say(std::string text);
		void Report(const FarmOutcome& outcome, const char* done);
		int Day() const { return m_Calendar ? m_Calendar->GetAbsoluteDay() : 0; }
		Season CurrentSeason() const { return m_Calendar ? m_Calendar->GetSeason() : Season::Spring; }
		Weather CurrentWeather() const { return m_Calendar ? m_Calendar->GetWeather() : Weather::Clear; }

	private:
		TileGrid& m_Grid;
		Inventory& m_Inventory;
		DirtyFn m_OnDirty;
		SleepFn m_OnSleep;
		InteractFn m_OnInteract;
		bool m_HoldToPlace = false;
		bool m_HoldArmed = false;
		int m_HoldX = -1;
		int m_HoldY = -1;
		const Calendar* m_Calendar = nullptr;
		CropId m_Seed = CropId::Parsnip;
		BuildMode m_Mode = BuildMode::Tools;
		BuildId m_Buildable = BuildId::WoodFence;
		int m_Rotation = 0;
		Aim m_Aim;
		PlaceSpec m_Spec;
		PlaceResult m_Preview = PlaceResult::Ok;
		std::string m_Message;
		uint32_t m_MessageSerial = 0;
	};

}
