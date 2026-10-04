#pragma once

namespace Dingo
{

	class GameAssets;

	bool RunAssetChecks(const GameAssets& assets);
	bool RunMovementChecks(const GameAssets& assets);
	bool RunCombatChecks(const GameAssets& assets);

}
