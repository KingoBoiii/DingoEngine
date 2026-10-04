#pragma once
#include <DingoEngine.h>

namespace Dingo
{

	struct MatchState;

	class TitleControllerScript : public ScriptableEntity
	{
	protected:
		void OnStart() override;
		void OnUpdate(float deltaTime) override;
		void OnDestroy() override;

	private:
		Font* m_Font = nullptr;
	};

	// Reads the run's result once, as it starts: the layer rebuilds this scene after every visit.
	class EndControllerScript : public ScriptableEntity
	{
	public:
		explicit EndControllerScript(const MatchState* match) : m_Match(match) {}

	protected:
		void OnStart() override;
		void OnUpdate(float deltaTime) override;
		void OnDestroy() override;

	private:
		const MatchState* m_Match;
		Font* m_Font = nullptr;
		float m_Age = 0.0f;
	};

}
