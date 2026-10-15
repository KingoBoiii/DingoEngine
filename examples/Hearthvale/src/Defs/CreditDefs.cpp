#include "Defs/CreditDefs.h"

#include <array>

namespace Dingo
{

	namespace
	{
		constexpr std::array k_Credits = {
			CreditEntry{ "cube-pets",        "Cube Pets",           "Kenney", "CC0", "kenney.nl" },
			CreditEntry{ "fantasy-town-kit", "Fantasy Town Kit",    "Kenney", "CC0", "kenney.nl" },
			CreditEntry{ "game-icons",       "Game Icons",          "Kenney", "CC0", "kenney.nl" },
			CreditEntry{ "interface-sounds", "Interface Sounds",    "Kenney", "CC0", "kenney.nl" },
			CreditEntry{ "nature-kit",       "Nature Kit",          "Kenney", "CC0", "kenney.nl" },
			CreditEntry{ "rpg-audio",        "RPG Audio",           "Kenney", "CC0", "kenney.nl" },
			CreditEntry{ "survival-kit",     "Survival Kit",        "Kenney", "CC0", "kenney.nl" },
			CreditEntry{ "ui-audio",         "UI Audio",            "Kenney", "CC0", "kenney.nl" },
			CreditEntry{ "ui-pack",          "UI Pack",             "Kenney", "CC0", "kenney.nl" },
			CreditEntry{ "kaykit",           "KayKit Adventurers",  "Kay Lousberg", "CC0", "kaylousberg.com" },
			CreditEntry{ "kaykit",           "KayKit Animations",   "Kay Lousberg", "CC0", "kaylousberg.com" },
			CreditEntry{ "",                 "Fredoka font",        "Fredoka Project Authors", "OFL-1.1", "fonts.google.com" },
			CreditEntry{ "",                 "Inter font",          "The Inter Project Authors", "OFL-1.1", "github.com/rsms/inter" },
			CreditEntry{ "",                 "DingoEngine",         "Victor Hansen", "MIT", "github.com/KingoBoiii" },
			CreditEntry{ "",                 "Engine libraries",    "See THIRD-PARTY-NOTICES.md", "Various", "github.com/KingoBoiii" },
		};
	}

	std::span<const CreditEntry> GetCredits()
	{
		return k_Credits;
	}

}
