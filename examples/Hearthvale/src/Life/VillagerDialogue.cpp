#include "Life/VillagerDialogue.h"
#include "Life/VillagerLines.h"

namespace Dingo
{

	VillagerDialogue::VillagerDialogue(const ValleyDirector& director, GameState& state)
		: m_Director(director), m_State(state)
	{
	}

	DialogueScript VillagerDialogue::Talk(VillagerId villager)
	{
		const Calendar& calendar = m_Director.GetCalendar();
		const int day = calendar.GetAbsoluteDay();
		if (day != m_TalkDay)
		{
			m_Talks.fill(0);
			m_TalkDay = day;
		}

		LineContext context;
		context.Season = calendar.GetSeason();
		context.Weather = calendar.GetWeather();
		context.Hearts = m_State.Bonds.GetHearts(villager);
		context.Day = day;
		context.Talk = m_Talks[static_cast<size_t>(villager)]++;
		m_State.Bonds.Talk(villager, day);

		DialogueScript script;
		script.Speaker = GetVillagerDef(villager).Name;
		script.Text = PickVillagerLine(villager, context);
		script.GiftTo = static_cast<int>(villager);
		return script;
	}

	DialogueScript VillagerDialogue::Gift(VillagerId villager)
	{
		const int day = m_Director.GetCalendar().GetAbsoluteDay();
		const ItemStack held = m_State.Pack.GetSelected();

		DialogueScript script;
		script.Speaker = GetVillagerDef(villager).Name;
		if (held.Empty())
		{
			script.Text = "Your hands are empty, farmer. Pick something from the hotbar first.";
			return script;
		}

		const GiftOutcome outcome = m_State.Bonds.Evaluate(villager, held.Item, day);
		if (outcome == GiftOutcome::Loved || outcome == GiftOutcome::Fine)
		{
			GiveGift(m_State.Pack, m_State.Board, villager, held.Item, day);
			m_State.Bonds.Give(villager, held.Item, day);
		}
		script.Text = GiftReaction(villager, outcome);
		return script;
	}

}
