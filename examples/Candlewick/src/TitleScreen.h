#pragma once
#include "Overlay.h"
#include "RunResult.h"

#include <DingoEngine.h>

namespace Dingo
{

	// Title: the game's name, a tagline, an enter prompt and the controls. Space/Enter/pad A or Start
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
		Entity m_Controls;
		Overlay::PadText m_PromptText{ [] { return Overlay::ConfirmPrompt("enter"); } };
		Overlay::PadText m_ControlsText{ &Overlay::ControlsSummary };
	};

	// End: shown once the altar burns, with the run's time and catches. Confirm or Esc returns to the
	// Title. The layer rebuilds this scene each time it is left, so OnStart reads a fresh result.
	class EndControllerScript : public ScriptableEntity
	{
	public:
		explicit EndControllerScript(const RunResult* result) : m_Result(result) {}

	protected:
		void OnStart() override;
		void OnUpdate(float deltaTime) override;
		void OnDestroy() override;

	private:
		const RunResult* m_Result = nullptr;
		Font* m_Font = nullptr;
		Entity m_Prompt;
		Overlay::PadText m_PromptText{ [] { return Overlay::ConfirmPrompt("return to the title"); } };
	};

}
