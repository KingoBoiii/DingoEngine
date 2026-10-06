#pragma once

namespace Dingo
{

	// What a finished run hands to the End screen. The layer owns it, so it outlives the Keep scene that fills it.
	struct RunResult
	{
		float Seconds = 0.0f;
		int Catches = 0;
		// The layer's frame delta; Scene::OnUpdate caps the one its scripts see, which would undercount a stalled frame.
		float FrameSeconds = 0.0f;
	};

}
