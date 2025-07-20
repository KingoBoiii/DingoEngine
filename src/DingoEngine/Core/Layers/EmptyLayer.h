#pragma once
#include "DingoEngine/Core/Layer.h"

#include "DingoEngine/Graphics/CommandList.h"

namespace Dingo
{
	class EmptyLayer : public Layer
	{
	public:
		EmptyLayer() : Layer("Empty Layer") {}
		virtual ~EmptyLayer() = default;

	public:
		virtual void OnUpdate(float deltaTime) override;
	};
}
