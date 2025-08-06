#pragma once
#include <DingoEngine.h>

namespace Dingo
{

	class GameLayer : public Layer
	{
	public:
		GameLayer() : Layer("Flappy Bird Game Layer")
		{}
		virtual ~GameLayer() = default;

	public:
		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate(float deltaTime) override;

#if DE_DEBUG
		virtual void OnImGuiRender() override;
#endif

	private:
	};

}
