#include "Placing/BuildController.h"
#include "GameTuning.h"

#include <algorithm>
#include <format>

namespace Dingo
{

	namespace
	{
		const char* ItemName(ItemId item)
		{
			return GetItemDef(item).Name;
		}

		std::string Cost(const BuildCost& cost)
		{
			std::string text;
			auto add = [&text](int count, const char* name)
			{
				if (count <= 0)
					return;
				if (!text.empty())
					text += ", ";
				text += std::format("{} {}", count, name);
			};
			add(cost.Coins, "coins");
			add(cost.Wood, "wood");
			add(cost.Stone, "stone");
			add(cost.Fibre, "fibre");
			return text.empty() ? "free" : text;
		}
	}

	BuildController::BuildController(TileGrid& grid, Inventory& inventory, DirtyFn onDirty)
		: m_Grid(grid), m_Inventory(inventory), m_OnDirty(std::move(onDirty))
	{}

	void BuildController::SelectTool(ItemId tool)
	{
		for (int i = 0; i < BUILD_TOOL_COUNT; ++i)
		{
			if (BUILD_TOOLS[i] == tool)
				m_ToolIndex = i;
		}
	}

	void BuildController::Say(std::string text)
	{
		m_Message = std::move(text);
		++m_MessageSerial;
	}

	void BuildController::Refresh(const AimInput& aim)
	{
		m_Aim = Targeting::Resolve(aim, TARGET_MOUSE_REACH_TILES);
		const BuildDef& def = GetBuildDef(m_Buildable);
		const Footprint footprint = RotatedFootprint(def, m_Rotation);
		const TileCoord origin = m_Aim.FromMouse
			? Targeting::OriginCentred(m_Aim.Target, footprint.W, footprint.H)
			: Targeting::OriginInFront(m_Aim.Target, m_Aim.Facing, footprint.W, footprint.H);
		m_Spec = PlaceSpec{ m_Buildable, origin.X, origin.Y, m_Rotation };
		m_Preview = CheckPlacement(m_Grid, m_Inventory, m_Spec);
	}

	void BuildController::CycleSelection(int direction)
	{
		if (m_Mode == BuildMode::Tools)
		{
			m_ToolIndex = (m_ToolIndex + direction + BUILD_TOOL_COUNT) % BUILD_TOOL_COUNT;
			return;
		}
		const int next = (static_cast<int>(m_Buildable) + direction + BUILD_COUNT) % BUILD_COUNT;
		m_Buildable = static_cast<BuildId>(next);
	}

	void BuildController::Step(const PlayerIntent& intent, const AimInput& aim)
	{
		if (intent.ToggleBuild)
		{
			m_Mode = m_Mode == BuildMode::Tools ? BuildMode::Build : BuildMode::Tools;
			Say(m_Mode == BuildMode::Build ? "Build mode" : "Tool mode");
		}
		if (intent.HotbarNext)
			CycleSelection(1);
		if (intent.HotbarPrevious)
			CycleSelection(-1);
		if (intent.HotbarSlot >= 0)
		{
			if (m_Mode == BuildMode::Tools && intent.HotbarSlot < BUILD_TOOL_COUNT)
				m_ToolIndex = intent.HotbarSlot;
			else if (m_Mode == BuildMode::Build && intent.HotbarSlot < BUILD_COUNT)
				m_Buildable = static_cast<BuildId>(intent.HotbarSlot);
		}
		if (intent.RotatePlacement && m_Mode == BuildMode::Build)
			m_Rotation = (m_Rotation + 1) & 3;

		Refresh(aim);

		if (intent.Use)
		{
			if (m_Mode == BuildMode::Build)
				Place();
			else
				UseTool();
		}
		if (intent.Interact && m_Mode == BuildMode::Build)
			Remove();
	}

	PlaceOutcome BuildController::Place()
	{
		const PlaceOutcome outcome = PlaceBuilding(m_Grid, m_Inventory, m_Spec);
		if (outcome.Result == PlaceResult::Ok)
		{
			m_OnDirty(outcome.Dirty);
			Say(std::format("Built {}", GetBuildDef(m_Spec.Id).Name));
		}
		else
		{
			Say(ToString(outcome.Result));
		}
		return outcome;
	}

	RemoveOutcome BuildController::Remove()
	{
		const RemoveOutcome outcome = RemoveBuilding(m_Grid, m_Inventory, m_Aim.Target.X, m_Aim.Target.Y);
		if (outcome.Removed)
		{
			m_OnDirty(outcome.Dirty);
			Say(std::format("Removed {} (refund {})", GetBuildDef(outcome.Id).Name, Cost(outcome.Refund)));
		}
		else
		{
			Say("Nothing to remove here");
		}
		return outcome;
	}

	ToolOutcome BuildController::UseTool()
	{
		const ItemId tool = GetTool();
		const ToolOutcome outcome = UseClearingTool(m_Grid, m_Inventory, tool, m_Aim.Target.X, m_Aim.Target.Y);
		switch (outcome.Result)
		{
			case ToolResult::Cleared:
				m_OnDirty(outcome.Dirty);
				if (outcome.YieldCount > 0)
					Say(std::format("{} +{} {}", GetDecorDef(outcome.Kind).Name, outcome.YieldCount, ItemName(outcome.Yield)));
				else
					Say(std::format("Cleared {}", GetDecorDef(outcome.Kind).Name));
				break;
			case ToolResult::WrongTool:
				Say(std::format("{} needs the {}", GetDecorDef(outcome.Kind).Name, ItemName(outcome.Needed)));
				break;
			case ToolResult::NotMine:
				Say(std::format("The {} is not ready yet", ItemName(tool)));
				break;
			case ToolResult::Nothing:
				break;
		}
		return outcome;
	}

}
