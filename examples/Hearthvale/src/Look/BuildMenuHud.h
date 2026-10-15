#pragma once
#include <DingoEngine.h>

#include "Placing/BuildController.h"

namespace Dingo
{

	// The temporary debug-text tool and build list; P8 replaces it with the hotbar and the build palette.
	// GPU side: never constructed headless.
	class BuildMenuHud
	{
	public:
		BuildMenuHud();

		// Declares the root; call once per frame.
		void Draw(const BuildController& controller, const Inventory& inventory, float deltaTime);

	private:
		void Line(const std::string& text, const glm::vec4& color);

	private:
		Font* m_Font = nullptr;
		uint32_t m_SeenSerial = 0;
		float m_MessageLeft = 0.0f;
	};

}
