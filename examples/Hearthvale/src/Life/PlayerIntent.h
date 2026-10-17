#pragma once
#include <glm/glm.hpp>

namespace Dingo
{

	// What the farmer wants this step: the keyboard and pad fill it, and the autoplay bot fills the same
	// struct instead. Move is relative to the camera (x right, y forward). The pulse fields are true for the one
	// step that follows a press; ClearPulses ends them.
	struct PlayerIntent
	{
		glm::vec2 Move{ 0.0f };

		bool Use = false;
		bool Interact = false;
		bool HotbarPrevious = false;
		bool HotbarNext = false;
		int HotbarSlot = -1;
		bool RotatePlacement = false;
		bool ToggleInventory = false;
		bool ToggleBuild = false;
		bool CycleSeed = false;
		bool Pause = false;
		// Level, not a pulse: true for as long as the use button is held. ClearPulses leaves it.
		bool UseHeld = false;

		int CameraSteps = 0;
		float CameraZoomAxis = 0.0f;
		float CameraZoomWheel = 0.0f;

		void ClearPulses()
		{
			Use = Interact = HotbarPrevious = HotbarNext = RotatePlacement = ToggleInventory = ToggleBuild = CycleSeed = Pause = false;
			HotbarSlot = -1;
			CameraSteps = 0;
			CameraZoomWheel = 0.0f;
		}
	};

}
