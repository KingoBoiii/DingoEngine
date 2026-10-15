#pragma once
#include "DingoEngine/Core/Layer.h"

namespace Dingo::Internal
{

	class DingoUIContext;

	// The game UI's place in the event order: an overlay above the game's layers and below
	// ImGui, so a press or scroll that lands on a blocking element stops here.
	class DingoUILayer : public Layer
	{
	public:
		explicit DingoUILayer(DingoUIContext& context)
			: Layer("DingoUI Layer"), m_Context(context)
		{}

		virtual void OnEvent(Event& e) override;

	private:
		DingoUIContext& m_Context;
	};

}
