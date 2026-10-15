#pragma once
#include "Farm/Calendar.h"
#include "Farm/Crops.h"
#include "Placing/Inventory.h"
#include "Placing/Placement.h"
#include "Placing/Targeting.h"
#include "Placing/Tools.h"
#include "Life/PlayerIntent.h"

#include <cstdint>
#include <functional>
#include <string>

namespace Dingo
{

	enum class BuildMode : uint8_t { Tools, Build };

	inline constexpr ItemId BUILD_TOOLS[] = { ItemId::Hoe, ItemId::WateringCan, ItemId::Axe, ItemId::Pickaxe, ItemId::Scythe };
	inline constexpr int BUILD_TOOL_COUNT = static_cast<int>(sizeof(BUILD_TOOLS) / sizeof(BUILD_TOOLS[0]));

	// The ToolsAndPlacement stage: what the farmer aims at, which tool or buildable is picked, and the swing, place
	// and remove actions the intent asks for. Plain C++ over the grid, so --check drives it headless. Every grid edit
	// is reported through `onDirty` with the tiles it touched; the layer forwards that to the ValleyWorld, so one edit
	// is one rebuilt cell. The tool slots stand in for P7's hotbar. In tool mode Use swings the selected tool (hoe
	// tills, can waters, scythe harvests a ripe crop, else clears) and Interact goes to bed at the farmhouse, harvests
	// a ripe crop or plants the selected seed.
	class BuildController
	{
	public:
		using DirtyFn = std::function<void(const TileRect&)>;
		using SleepFn = std::function<void()>;

		BuildController(TileGrid& grid, Inventory& inventory, DirtyFn onDirty);

		// Resolves the aim, then applies the intent's selection, rotate, use and interact pulses in that order.
		void Step(const PlayerIntent& intent, const AimInput& aim);

		BuildMode GetMode() const { return m_Mode; }
		ItemId GetTool() const { return BUILD_TOOLS[m_ToolIndex]; }
		BuildId GetBuildable() const { return m_Buildable; }
		int GetRotation() const { return m_Rotation; }
		const Aim& GetAim() const { return m_Aim; }
		// The spec Use would place now; valid in either mode.
		PlaceSpec GetSpec() const { return m_Spec; }
		PlaceResult GetPreview() const { return m_Preview; }
		const std::string& GetMessage() const { return m_Message; }
		uint32_t GetMessageSerial() const { return m_MessageSerial; }

		void SetMode(BuildMode mode) { m_Mode = mode; }
		void SelectTool(ItemId tool);
		void SelectBuildable(BuildId id) { m_Buildable = id; }
		void SetRotation(int quarterTurns) { m_Rotation = quarterTurns & 3; }

		// The calendar gives the day, season and weather the farm actions use; without one it is a clear spring day 0.
		void SetCalendar(const Calendar* calendar) { m_Calendar = calendar; }
		void SetSleepHandler(SleepFn onSleep) { m_OnSleep = std::move(onSleep); }
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
		const Calendar* m_Calendar = nullptr;
		CropId m_Seed = CropId::Parsnip;
		BuildMode m_Mode = BuildMode::Tools;
		int m_ToolIndex = 2;
		BuildId m_Buildable = BuildId::WoodFence;
		int m_Rotation = 0;
		Aim m_Aim;
		PlaceSpec m_Spec;
		PlaceResult m_Preview = PlaceResult::Ok;
		std::string m_Message;
		uint32_t m_MessageSerial = 0;
	};

}
