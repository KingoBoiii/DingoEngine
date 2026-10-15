#pragma once
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
	// is one rebuilt cell. The tool slots stand in for P7's hotbar.
	class BuildController
	{
	public:
		using DirtyFn = std::function<void(const TileRect&)>;

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

		// The actions themselves, for the bot as well as the intent.
		PlaceOutcome Place();
		RemoveOutcome Remove();
		ToolOutcome UseTool();

	private:
		void Refresh(const AimInput& aim);
		void CycleSelection(int direction);
		void Say(std::string text);

	private:
		TileGrid& m_Grid;
		Inventory& m_Inventory;
		DirtyFn m_OnDirty;
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
