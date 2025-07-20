#pragma once
#include "GraphicsTest.h"

#include <DingoEngine.h>

namespace Dingo
{
	class ClearColorTest : public GraphicsTest
	{
	public:
		ClearColorTest() = default;
		virtual ~ClearColorTest() = default;

	public:
		virtual void Update(float deltaTime) override;

		virtual void ImGuiRender() override;
		
	};

} // namespace Dingo
