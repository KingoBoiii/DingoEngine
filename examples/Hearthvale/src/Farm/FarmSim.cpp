#include "Farm/FarmSim.h"
#include "GameTuning.h"

namespace Dingo
{

	FarmSim::FarmSim(ValleyDirector& director, Inventory& inventory)
		: m_Director(director), m_Inventory(inventory)
	{
		m_Director.OnDayRoll(DayRollStep::Crops, [this](ValleyDirector& d, DayRollReason reason)
		{
			m_Last = FarmRollReport{};
			m_Last.Reason = reason;
			if (reason == DayRollReason::PassOut)
			{
				const int lost = m_Inventory.GetCoins() * PASSOUT_COIN_LOSS_PERMILLE / 1000;
				if (lost > 0 && m_Inventory.SpendCoins(lost))
					m_Last.CoinsLost = lost;
			}
			const Calendar& calendar = d.GetCalendar();
			m_Last.Crops = GrowCrops(d.GetGrid(), calendar.GetAbsoluteDay(), calendar.GetWeather());
			m_Last.Dried = DrySoil(d.GetGrid());
		});
		m_Director.OnDayRoll(DayRollStep::Sprinklers, [this](ValleyDirector& d, DayRollReason)
		{
			m_Last.Sprinklered = WaterWithSprinklers(d.GetGrid());
		});
		m_Director.OnDayRoll(DayRollStep::Weather, [this](ValleyDirector& d, DayRollReason)
		{
			if (d.GetCalendar().GetWeather() == Weather::Rain)
				m_Last.Rained = WaterOutdoorSoil(d.GetGrid());
		});
	}

}
