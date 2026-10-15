#include "Farm/GameState.h"
#include "Sim/Fnv.h"

namespace Dingo
{

	GameStateDigest HashGameState(const ValleyDirector& director, const GameState& state)
	{
		const Calendar& calendar = director.GetCalendar();

		Fnv1a time;
		time.I32(calendar.GetAbsoluteDay());
		time.I32(calendar.GetTickOfDay());
		time.Byte(static_cast<uint8_t>(calendar.GetWeather()));

		GameStateDigest digest;
		digest.Grid = director.GetGrid().Hash();
		digest.Pack = state.Pack.Hash();
		digest.Bin = state.Bin.Hash();
		digest.Board = state.Board.Hash();
		digest.Time = time.Value();

		Fnv1a all;
		all.U64(digest.Grid);
		all.U64(digest.Pack);
		all.U64(digest.Bin);
		all.U64(digest.Board);
		all.U64(digest.Time);
		digest.All = all.Value();
		return digest;
	}

}
