#pragma once
#include <span>

namespace Dingo
{

	// One line of the credits screen. This table is the only place the credits live: a pack added to the game adds a row.
	struct CreditEntry
	{
		// The pack's folder under assets/kenney or assets/kaykit, "" for anything that has none.
		const char* Folder;
		const char* Pack;
		const char* Author;
		const char* Licence;
		const char* Url;
	};

	std::span<const CreditEntry> GetCredits();

}
