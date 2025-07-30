#pragma once
#include "RendererTest.h"

namespace Dingo
{

	class StaticTriangleTest : public RendererTest
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
