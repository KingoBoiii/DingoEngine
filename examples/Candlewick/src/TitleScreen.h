#pragma once
#include <DingoEngine.h>

namespace Dingo
{

	// Title: the game's name, a tagline and an enter prompt. Space/Enter/pad A or Start
	// requests the Keep; Esc closes the application.
	class TitleControllerScript : public ScriptableEntity
	{
	protected:
		void OnStart() override;
		void OnUpdate(float deltaTime) override;
		void OnDestroy() override;

	private:
		Font* m_Font = nullptr;
		Entity m_Prompt;
	};

	// End: shown once the altar burns. Confirm or Esc returns to the Title.
	class EndControllerScript : public ScriptableEntity
	{
	protected:
		void OnStart() override;
		void OnUpdate(float deltaTime) override;
		void OnDestroy() override;

	private:
		Font* m_Font = nullptr;
		Entity m_Prompt;
	};

}
