#pragma once
#include "DingoEngine/Core/Layer.h"

#include "DingoEngine/Graphics/CommandList.h"

namespace DingoEngine
{
	class EmptyLayer : public Layer
	{
	public:
		EmptyLayer() : Layer("Empty Layer") {}
		virtual ~EmptyLayer() = default;

	public:
		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate() override;
	private:
		CommandList* m_CommandList = nullptr;
	};
}
