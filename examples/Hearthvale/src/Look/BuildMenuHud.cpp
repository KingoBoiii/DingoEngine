#include "Look/BuildMenuHud.h"
#include "GameTuning.h"

#include <algorithm>
#include <format>

namespace Dingo
{

	namespace
	{
		constexpr const char* k_FontPath = "fonts/Inter-Bold.ttf";
		constexpr float k_Margin = 16.0f;
		constexpr float k_PaddingX = 14.0f;
		constexpr float k_PaddingY = 8.0f;

		std::string CostText(const BuildCost& cost)
		{
			std::string text;
			auto add = [&text](int count, const char* name)
			{
				if (count > 0)
					text += std::format("{}{} {}", text.empty() ? "" : ", ", count, name);
			};
			add(cost.Coins, "coins");
			add(cost.Wood, "wood");
			add(cost.Stone, "stone");
			add(cost.Fibre, "fibre");
			return text.empty() ? "free" : text;
		}
	}

	BuildMenuHud::BuildMenuHud()
	{
		AssetManager& assets = Application::Get().GetAssetManager();
		m_Font = assets.GetFont(assets.Load(k_FontPath));
		if (!m_Font)
		{
			DE_ERROR("Hearthvale: failed to load font '{}'", k_FontPath);
		}
	}

	void BuildMenuHud::Line(const std::string& text, const glm::vec4& color)
	{
		DingoUI::Text(text, UITextParams().SetSize(HUD_MENU_SIZE).SetColor(color).SetWrap(false).SetFont(m_Font));
	}

	void BuildMenuHud::Draw(const BuildController& controller, const Inventory& inventory, float deltaTime)
	{
		if (!m_Font)
			return;

		if (controller.GetMessageSerial() != m_SeenSerial)
		{
			m_SeenSerial = controller.GetMessageSerial();
			m_MessageLeft = HUD_MESSAGE_SECONDS;
		}
		m_MessageLeft = (std::max)(0.0f, m_MessageLeft - deltaTime);

		if (!DingoUI::BeginRoot("build_menu", UIRootParams().SetAnchor(UIAnchor::BottomLeft).SetOffset({ k_Margin, -k_Margin }).SetBlocksInput(false)
			.SetLayout(UIBoxParams().SetPadding(k_PaddingX, k_PaddingY).SetBackground(HUD_PANEL_COLOR))))
			return;

		Line(std::format("Coins {}   Wood {}   Stone {}   Fibre {}", inventory.GetCoins(), inventory.Count(ItemId::Wood),
			inventory.Count(ItemId::Stone), inventory.Count(ItemId::Fibre)), HUD_TEXT_COLOR);

		if (controller.GetMode() == BuildMode::Tools)
		{
			Line(std::format("Tool: {}   [1-5 / wheel] switch   [Space / click] use   [F] plant / harvest / sleep   [B] build", GetItemDef(controller.GetTool()).Name), HUD_TEXT_PICK_COLOR);
			const CropId seed = controller.GetSeed();
			if (seed < CropId::Count)
				Line(std::format("Seed: {} x{}   [T] next", GetCropDef(seed).Name, inventory.Count(SeedItem(seed))), HUD_TEXT_COLOR);
			else
				Line("Seed: none for this season", HUD_TEXT_DIM_COLOR);
		}
		else
		{
			Line("Build   [wheel / 1-9] pick   [R] rotate   [Space / click] place   [F / right click] remove   [B] tools", HUD_TEXT_PICK_COLOR);
			const int selected = static_cast<int>(controller.GetBuildable());
			const int first = std::clamp(selected - HUD_MENU_ROWS / 2, 0, (std::max)(0, BUILD_COUNT - HUD_MENU_ROWS));
			const int last = (std::min)(BUILD_COUNT, first + HUD_MENU_ROWS);
			for (int i = first; i < last; ++i)
			{
				const BuildDef& def = GetBuildDef(static_cast<BuildId>(i));
				const bool affordable = CanAfford(inventory, def.Cost);
				const std::string text = std::format("{} {}  ({}x{})  {}", i == selected ? ">" : " ", def.Name, def.Width, def.Height, CostText(def.Cost));
				Line(text, i == selected ? HUD_TEXT_PICK_COLOR : (affordable ? HUD_TEXT_COLOR : HUD_TEXT_DIM_COLOR));
			}
			const PlaceResult preview = controller.GetPreview();
			Line(std::format("{}   rotation {}", preview == PlaceResult::Ok ? "Ready to place" : ToString(preview), controller.GetRotation() * 90), HUD_TEXT_COLOR);
		}

		if (m_MessageLeft > 0.0f)
			Line(controller.GetMessage(), HUD_TEXT_COLOR);
		DingoUI::EndRoot();
	}

}
