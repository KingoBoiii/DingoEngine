#pragma once
#include "Farm/GameState.h"
#include "Ui/MenuPages.h"

#include <array>

namespace Dingo
{

	// What talking to a villager and giving one a gift do to the state, and the words that come back. Plain C++ over
	// GameState and the calendar, so --check drives it without a screen.
	class VillagerDialogue
	{
	public:
		VillagerDialogue(const ValleyDirector& director, GameState& state);

		// Friendship bookkeeping for a chat, then the line for today's season, weather and friendship.
		DialogueScript Talk(VillagerId villager);
		// Gives the selected hotbar item; the reply says how it landed.
		DialogueScript Gift(VillagerId villager);

	private:
		const ValleyDirector& m_Director;
		GameState& m_State;
		int m_TalkDay = -1;
		std::array<int, VILLAGER_COUNT> m_Talks{};
	};

}
