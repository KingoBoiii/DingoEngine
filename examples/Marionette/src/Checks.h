#pragma once

namespace Dingo
{

	class GameAssets;
	class ReachTable;

	bool RunAssetChecks(const GameAssets& assets);
	bool RunMovementChecks(const GameAssets& assets);
	bool RunCombatChecks(const GameAssets& assets);
	bool RunAiChecks(const GameAssets& assets, const ReachTable& reach);

}
