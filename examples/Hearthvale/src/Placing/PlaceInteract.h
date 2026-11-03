#pragma once
#include "Farm/GameState.h"

#include <string>

namespace Dingo
{

	struct PlaceInteraction
	{
		bool Handled = false;
		std::string Message;
	};

	// What the Interact press does on tile (x, y) beyond the landmarks: tend a coop or barn (collect the produce, feed
	// the animals), fill the watering can at the well, look at the pond. Changes the pack and the herd, nothing else.
	// `animalsEnabled` false leaves pens alone, as if the animals did not exist.
	PlaceInteraction InteractWithPlace(const TileGrid& grid, GameState& state, int x, int y, bool animalsEnabled = ANIMALS_ENABLED);

}
