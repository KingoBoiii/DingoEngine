#pragma once
#include "RendererTest.h"

namespace Dingo
{

	class StaticTriangleTest : public RendererTest
	{
	public:
		StaticTriangleTest(Renderer* renderer)
			: RendererTest(renderer)
		{}
		virtual ~StaticTriangleTest() = default;

	public:
		virtual void Initialize() override;
		virtual void Update(float deltaTime) override;
		virtual void Cleanup() override;

	private:
		Shader* m_Shader = nullptr;
		Pipeline* m_Pipeline = nullptr;
	};

}
