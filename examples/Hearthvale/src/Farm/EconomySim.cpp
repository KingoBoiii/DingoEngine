#include "Farm/EconomySim.h"

namespace Dingo
{

	EconomySim::EconomySim(ValleyDirector& director, GameState& state)
		: m_State(state)
	{
		director.OnDayRoll(DayRollStep::Shipping, [this](ValleyDirector& d, DayRollReason)
		{
			m_Last = EconomyRollReport{};
			const int day = d.GetCalendar().GetAbsoluteDay();
			m_Last.Shipped = m_State.Bin.Settle(m_State.Pack);
			for (int i = 0; i < ITEM_COUNT; ++i)
				m_State.Board.NoteShipped(static_cast<ItemId>(i), m_Last.Shipped.Items[static_cast<size_t>(i)], day);
			m_State.Board.NoteEarned(m_Last.Shipped.Coins, day);
		});
		director.OnDayRoll(DayRollStep::Board, [this](ValleyDirector& d, DayRollReason)
		{
			m_Last.Board = m_State.Board.Settle(d.GetGrid(), d.GetCalendar().GetAbsoluteDay());
			m_State.Pack.AddCoins(m_Last.Board.RewardCoins);
		});
	}

}
