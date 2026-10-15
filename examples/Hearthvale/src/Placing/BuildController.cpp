#include "Placing/BuildController.h"
#include "GameTuning.h"
#include "Placing/Interactions.h"

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

	ItemId BuildController::GetHeld() const
	{
		const ItemStack& held = m_Inventory.GetSelected();
		return held.Empty() ? ItemId::Count : held.Item;
	}

	ItemId BuildController::GetTool() const
	{
		const ItemId held = GetHeld();
		return held < ItemId::Count && GetItemDef(held).Kind == ItemKind::Tool ? held : ItemId::Count;
	}

	void BuildController::SelectTool(ItemId tool)
	{
		const int slot = m_Inventory.FindSlot(tool);
		if (slot >= 0 && slot < HOTBAR_SLOTS)
			m_Inventory.SelectSlot(slot);
	}

	void BuildController::SyncSeedToHeld()
	{
		const ItemId held = GetHeld();
		if (held < ItemId::Count && GetItemDef(held).Kind == ItemKind::Seed)
			m_Seed = GetItemDef(held).Crop;
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
			m_Inventory.CycleSelection(direction);
			SyncSeedToHeld();
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
			if (m_Mode == BuildMode::Tools && intent.HotbarSlot < HOTBAR_SLOTS)
			{
				m_Inventory.SelectSlot(intent.HotbarSlot);
				SyncSeedToHeld();
			}
			else if (m_Mode == BuildMode::Build && intent.HotbarSlot < BUILD_COUNT)
				m_Buildable = static_cast<BuildId>(intent.HotbarSlot);
		}
		if (intent.CycleSeed && m_Mode == BuildMode::Tools)
			CycleSeed();
		if (intent.RotatePlacement && m_Mode == BuildMode::Build)
			m_Rotation = (m_Rotation + 1) & 3;

		Refresh(aim);

		if (intent.Use)
		{
			if (m_Mode == BuildMode::Build)
			{
				Place();
				m_HoldArmed = m_HoldToPlace;
				m_HoldX = m_Aim.Target.X;
				m_HoldY = m_Aim.Target.Y;
			}
			else
				UseSelectedTool();
		}
		else if (m_HoldArmed && m_Mode == BuildMode::Build && intent.UseHeld)
		{
			if (m_Aim.Target.X != m_HoldX || m_Aim.Target.Y != m_HoldY)
			{
				Place();
				m_HoldX = m_Aim.Target.X;
				m_HoldY = m_Aim.Target.Y;
			}
		}
		if (!intent.UseHeld || m_Mode != BuildMode::Build)
			m_HoldArmed = false;
		if (intent.Interact)
		{
			if (m_Mode == BuildMode::Build)
				Remove();
			else
				InteractTool();
		}
	}

	void BuildController::SetHoldToPlace(bool enabled)
	{
		m_HoldToPlace = enabled;
		if (!enabled)
			m_HoldArmed = false;
	}

	PlaceOutcome BuildController::Place()
	{
		const PlaceOutcome outcome = PlaceBuilding(m_Grid, m_Inventory, m_Spec);
		if (outcome.Result == PlaceResult::Ok)
		{
			m_OnDirty(outcome.Dirty);
			Say(std::format("Built {}", GetBuildDef(m_Spec.Id).Name));
			Notify(ActionFeedback::Place);
		}
		else
		{
			Say(ToString(outcome.Result));
			Notify(ActionFeedback::Denied);
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
			Notify(ActionFeedback::Remove);
		}
		else if (outcome.InventoryFull)
		{
			Say("Inventory full");
			Notify(ActionFeedback::Denied);
		}
		else
		{
			Say("Nothing to remove here");
			Notify(ActionFeedback::Denied);
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
				Notify(tool == ItemId::Axe ? ActionFeedback::Chop : tool == ItemId::Pickaxe ? ActionFeedback::Mine : ActionFeedback::Cut);
				break;
			case ToolResult::WrongTool:
				Say(std::format("{} needs the {}", GetDecorDef(outcome.Kind).Name, ItemName(outcome.Needed)));
				Notify(ActionFeedback::Denied);
				break;
			case ToolResult::InventoryFull:
				Say("Inventory full");
				Notify(ActionFeedback::Denied);
				break;
			case ToolResult::NotMine:
				Say(std::format("The {} clears nothing", ItemName(tool)));
				break;
			case ToolResult::Nothing:
				break;
		}
		return outcome;
	}

	void BuildController::CycleSeed()
	{
		const int first = m_Seed < CropId::Count ? static_cast<int>(m_Seed) : 0;
		for (int i = 1; i <= CROP_COUNT; ++i)
		{
			const CropId crop = static_cast<CropId>((first + i) % CROP_COUNT);
			if (m_Inventory.Count(SeedItem(crop)) > 0)
			{
				m_Seed = crop;
				Say(std::format("Seed: {} ({})", GetCropDef(crop).Name, m_Inventory.Count(SeedItem(crop))));
				return;
			}
		}
		Say("No seeds");
	}

	CropId BuildController::GetSeed() const
	{
		if (m_Seed < CropId::Count && m_Inventory.Count(SeedItem(m_Seed)) > 0)
			return m_Seed;
		return PickSeed(m_Inventory, m_Seed, CurrentSeason());
	}

	void BuildController::Report(const FarmOutcome& outcome, const char* done, ActionFeedback feedback)
	{
		if (outcome.Result == FarmResult::Ok)
		{
			if (outcome.Dirty.W > 0 && outcome.Dirty.H > 0)
				m_OnDirty(outcome.Dirty);
			Say(done);
			Notify(feedback);
		}
		else if (outcome.Result != FarmResult::OutOfBounds)
		{
			Say(ToString(outcome.Result));
			Notify(ActionFeedback::Denied);
		}
	}

	FarmOutcome BuildController::Till()
	{
		const FarmOutcome outcome = TillTile(m_Grid, m_Aim.Target.X, m_Aim.Target.Y, CurrentWeather());
		Report(outcome, "Tilled", ActionFeedback::Till);
		return outcome;
	}

	FarmOutcome BuildController::Water()
	{
		const FarmOutcome outcome = WaterTile(m_Grid, m_Aim.Target.X, m_Aim.Target.Y);
		Report(outcome, "Watered", ActionFeedback::Water);
		return outcome;
	}

	FarmOutcome BuildController::Harvest()
	{
		const FarmOutcome outcome = HarvestTile(m_Grid, m_Inventory, m_Aim.Target.X, m_Aim.Target.Y);
		if (outcome.Result == FarmResult::Ok)
		{
			m_OnDirty(outcome.Dirty);
			Say(std::format("{} +{} {}", GetCropDef(outcome.Crop).Name, outcome.YieldCount, ItemName(outcome.Yield)));
			Notify(ActionFeedback::Harvest);
		}
		else if (outcome.Result != FarmResult::OutOfBounds)
		{
			Say(ToString(outcome.Result));
			Notify(ActionFeedback::Denied);
		}
		return outcome;
	}

	FarmOutcome BuildController::Plant()
	{
		const CropId seed = GetSeed();
		if (seed >= CropId::Count)
		{
			Say("No seeds for this season");
			Notify(ActionFeedback::Denied);
			FarmOutcome none;
			none.Result = FarmResult::NoSeed;
			return none;
		}
		const FarmOutcome outcome = PlantSeed(m_Grid, m_Inventory, Day(), CurrentSeason(), m_Aim.Target.X, m_Aim.Target.Y, seed);
		Report(outcome, std::format("Planted {}", GetCropDef(seed).Name).c_str(), ActionFeedback::Plant);
		return outcome;
	}

	void BuildController::UseSelectedTool()
	{
		const ItemId held = GetHeld();
		if (held < ItemId::Count && GetItemDef(held).Kind == ItemKind::Seed)
		{
			SyncSeedToHeld();
			Plant();
			return;
		}
		switch (GetTool())
		{
			case ItemId::Count:
				break;
			case ItemId::Hoe:
				Till();
				break;
			case ItemId::WateringCan:
				Water();
				break;
			case ItemId::Scythe:
				if (m_Grid.InBounds(m_Aim.Target.X, m_Aim.Target.Y) && IsRipe(m_Grid.At(m_Aim.Target.X, m_Aim.Target.Y).Crop))
					Harvest();
				else
					UseTool();
				break;
			default:
				UseTool();
				break;
		}
	}

	bool BuildController::InteractTool()
	{
		const int x = m_Aim.Target.X;
		const int y = m_Aim.Target.Y;
		if (!m_Grid.InBounds(x, y))
			return false;

		if (m_OnInteract)
		{
			const std::optional<Landmark> landmark = LandmarkAt(m_Grid, x, y);
			if (landmark && *landmark != Landmark::FarmHouse && m_OnInteract(*landmark))
				return true;
		}

		if (m_OnTileInteract && m_OnTileInteract(x, y))
			return true;

		const LandmarkSpec& home = GetLandmarkSpec(Landmark::FarmHouse);
		if (home.Footprint.Contains(x, y) || (x == home.DoorX && y == home.DoorY))
		{
			if (!m_OnSleep)
				return false;
			Say("Going to bed");
			m_OnSleep();
			return true;
		}

		const Tile& tile = m_Grid.At(x, y);
		if (tile.Crop.Present())
			return Harvest().Result == FarmResult::Ok;
		if (tile.GroundState == Ground::Tilled || tile.GroundState == Ground::Watered)
			return Plant().Result == FarmResult::Ok;
		return false;
	}

}
