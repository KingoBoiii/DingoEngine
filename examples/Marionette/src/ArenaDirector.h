#pragma once
#include <DingoEngine.h>

#include <memory>
#include <vector>

namespace Dingo
{

	class ArenaWorld;
	class CameraRig;
	class Fighter;
	class GameAssets;

	class ArenaDirectorScript : public ScriptableEntity
	{
	public:
		explicit ArenaDirectorScript(const GameAssets* assets);
		~ArenaDirectorScript() override;

	protected:
		void OnStart() override;
		void OnUpdate(float deltaTime) override;
		void OnDestroy() override;

	private:
		const GameAssets* m_Assets = nullptr;
		std::unique_ptr<ArenaWorld> m_World;
		std::vector<std::unique_ptr<Fighter>> m_Fighters;
		std::unique_ptr<CameraRig> m_Camera;
	};

}
