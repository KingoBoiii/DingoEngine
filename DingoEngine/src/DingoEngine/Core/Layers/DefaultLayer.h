#pragma once
#include "DingoEngine/Core/Layer.h"

#include "DingoEngine/Graphics/CommandList.h"

namespace DingoEngine
{
	class DefaultLayer : public Layer
	{
	public:
		DefaultLayer() : Layer("Default Layer") {}
		virtual ~DefaultLayer() = default;

	public:
		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate() override;
	private:
		CommandList* m_CommandList = nullptr;
	};
}
