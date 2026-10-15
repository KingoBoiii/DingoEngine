#pragma once
#include "Life/PlayerIntent.h"

namespace Dingo
{

	// Reads the keyboard, mouse and pad of 2.3 once a frame. Held fields (Move, CameraZoomAxis) are replaced;
	// pulse fields are or-ed in, so a press in a frame that runs no sim step survives until one does.
	void PollPlayerIntent(PlayerIntent& intent);

	// The stick and key mix without a window or a pad: `keys` is x right, y forward in {-1, 0, 1}.
	glm::vec2 CombineMove(const glm::vec2& keys, const glm::vec2& stick, float deadzone);

}
