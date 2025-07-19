#pragma once
#include "GraphicsTest.h"

#include <DingoEngine.h>

namespace Dingo
{

	class StaticTriangleTest : public GraphicsTest
	{
	public:
		virtual ~StaticTriangleTest() = default;

	public:
		virtual void InitializeGraphics() override;
		virtual void Update(float deltaTime) override;
		virtual void CleanupGraphics() override;

	private:
		Shader* m_Shader = nullptr;
		Pipeline* m_Pipeline = nullptr;
	};

}
