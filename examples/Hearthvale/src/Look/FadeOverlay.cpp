#include "Look/FadeOverlay.h"
#include "GameTuning.h"

namespace Dingo
{

	void FadeOverlay::Draw(float alpha)
	{
		if (alpha <= 0.001f)
			return;

		if (!DingoUI::BeginRoot("fade", UIRootParams().SetAnchor(UIAnchor::Stretch).SetLayer(FADE_UI_LAYER).SetBlocksInput(false)
			.SetLayout(UIBoxParams().SetBackground(glm::vec4(0.0f, 0.0f, 0.0f, alpha)))))
			return;
		DingoUI::EndRoot();
	}

}
