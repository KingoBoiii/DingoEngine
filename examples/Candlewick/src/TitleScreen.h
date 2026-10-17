#pragma once
#include "Overlay.h"
#include "RunResult.h"

#include <DingoEngine.h>

namespace Dingo
{

	// Title: the game's name, a tagline, an enter button and the controls, with a quit button below. Space,
	// Enter, pad A or Start, or the enter button, requests the Keep; Esc or the quit button closes the application.
	class TitleControllerScript : public ScriptableEntity
	{
	protected:
		void OnStart() override;
		void OnUpdate(float deltaTime) override;

	private:
		Overlay::PadText m_PromptText{ [] { return Overlay::ConfirmPrompt("enter"); } };
		Overlay::PadText m_ControlsText{ &Overlay::ControlsSummary };
	};

	// End: shown once the altar burns, with the run's time and catches. Confirm, Esc or the return button
	// goes back to the Title. The layer rebuilds this scene each time it is left, so OnStart reads a fresh result.
	class EndControllerScript : public ScriptableEntity
	{
	public:
		explicit EndControllerScript(const RunResult* result) : m_Result(result) {}

	protected:
		void OnStart() override;
		void OnUpdate(float deltaTime) override;

	private:
		const RunResult* m_Result = nullptr;
		Overlay::PadText m_PromptText{ [] { return Overlay::ConfirmPrompt("return to the title"); } };
	};

}
