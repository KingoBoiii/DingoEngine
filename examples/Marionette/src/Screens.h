#pragma once
#include <DingoEngine.h>

#include <memory>

namespace Dingo
{

	class GameAssets;
	class Showcase;
	struct MatchState;

	// The four fighters idling in a row behind the name.
	class TitleControllerScript : public ScriptableEntity
	{
	public:
		explicit TitleControllerScript(const GameAssets* assets);
		~TitleControllerScript() override;

	protected:
		void OnStart() override;
		void OnUpdate(float deltaTime) override;
		void OnDestroy() override;

	private:
		const GameAssets* m_Assets;
		std::unique_ptr<Showcase> m_Showcase;
		float m_Age = 0.0f;
	};

	// Reads the run's result once, as it starts: the layer rebuilds this scene after every visit. A win shows the
	// Knight taunting in front of the camera.
	class EndControllerScript : public ScriptableEntity
	{
	public:
		EndControllerScript(const MatchState* match, const GameAssets* assets);
		~EndControllerScript() override;

	protected:
		void OnStart() override;
		void OnUpdate(float deltaTime) override;
		void OnDestroy() override;

	private:
		const MatchState* m_Match;
		const GameAssets* m_Assets;
		std::unique_ptr<Showcase> m_Showcase;
		float m_Age = 0.0f;
	};

}
