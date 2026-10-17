#pragma once
#include "Farm/GameState.h"
#include "GameTuning.h"
#include "ValleyDirector.h"
#include "World/TileGrid.h"

#include <DingoEngine/Core/SaveData.h>

#include <cstdint>
#include <string>

namespace Dingo
{

	// Bump it, and add a step to MigrateSave, whenever the layout changes or an id table (crops, builds, decor,
	// landmarks), which saves by number, is reordered. Items save by name.
	inline constexpr int SAVE_SCHEMA = 1;

	struct SaveMeta
	{
		double PlaySeconds = 0.0;
	};

	// A save read and checked in full, apart from the running game, so a broken file is never half-applied.
	struct LoadedGame
	{
		uint32_t Seed = 1;
		TileGrid Grid{ VALLEY_SIZE, VALLEY_SIZE };
		int Day = 0;
		int Tick = 0;
		Weather Today = Weather::Clear;
		GameState State;
		SaveMeta Meta;
	};

	SaveData WriteSave(const ValleyDirector& director, const GameState& state, const SaveMeta& meta);

	// The schema a save says it has; 0 when it has none (the first, unversioned layout), -1 when it is not a number.
	int SaveSchemaOf(const SaveData& save);

	// Upgrades `save` in place, one step per schema, to SAVE_SCHEMA. Refuses a newer schema and a malformed older one.
	bool MigrateSave(SaveData& save, std::string& error);

	// Migrates a copy and checks every field; `out` changes only on success.
	bool ReadSave(const SaveData& save, LoadedGame& out, std::string& error);

	// Swaps the game in. `state` is assigned in place, so the sims holding references to it keep them.
	void ApplyLoadedGame(LoadedGame&& game, ValleyDirector& director, GameState& state);

	// Day 1 of spring at 6:00 in the valley `seed` generates, with the starter loadout.
	LoadedGame MakeNewGame(uint32_t seed);

	// "Spring 5, year 1 - 812 coins - 2h 15m played", for the slots screen. ASCII only.
	std::string DescribeGame(const LoadedGame& game);

}
