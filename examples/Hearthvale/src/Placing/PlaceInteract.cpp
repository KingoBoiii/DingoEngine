#include "Placing/PlaceInteract.h"

#include <format>

namespace Dingo
{

	namespace
	{
		std::string Plural(int count, const char* noun)
		{
			return std::format("{} {}{}", count, noun, count == 1 ? "" : "s");
		}

		std::string TendMessage(const TendResult& tend)
		{
			const char* animal = AnimalName(tend.Kind);
			const char* produce = GetItemDef(ProduceItem(tend.Kind)).Name;
			if (tend.Residents == 0)
				return std::format("Nobody lives here yet. {}s move in overnight.", tend.Kind == AnimalKind::Cow ? "Cow" : "Chicken");

			std::string text;
			if (tend.Collected > 0)
				text = std::format("Collected {} {}", tend.Collected, produce);
			if (tend.Fed > 0)
			{
				text += text.empty() ? "Fed " : ", fed ";
				text += Plural(tend.Fed, animal);
			}
			if (tend.Blocked > 0)
			{
				text += text.empty() ? "" : ". ";
				text += std::format("Pack full, {} {} left behind", tend.Blocked, produce);
			}
			if (text.empty())
				text = std::format("The {}s are fed and content", animal);
			return text;
		}
	}

	PlaceInteraction InteractWithPlace(const TileGrid& grid, GameState& state, int x, int y, bool animalsEnabled)
	{
		PlaceInteraction result;
		if (!grid.InBounds(x, y))
			return result;

		const Tile& tile = grid.At(x, y);
		if (tile.Occupant != k_NoOccupant)
		{
			const Occupant* occupant = grid.FindOccupant(tile.Occupant);
			if (occupant && IsAnimalPen(*occupant) && animalsEnabled)
			{
				result.Handled = true;
				result.Message = TendMessage(TendPen(grid, state.Animals, state.Pack, occupant->Id));
			}
			else if (occupant && occupant->Kind == OccupantKind::Building && occupant->DefId == static_cast<uint16_t>(BuildId::Well))
			{
				result.Handled = true;
				result.Message = state.Pack.Count(ItemId::WateringCan) > 0
					? "You fill the watering can at the well"
					: "The well is deep and cool. You have no watering can";
			}
			return result;
		}

		if (tile.GroundState == Ground::Water)
		{
			result.Handled = true;
			result.Message = "The pond is still and clear. Fishing is not ready yet";
		}
		return result;
	}

}
