#pragma once
#include "Renderer2DTest.h"

namespace Dingo
{

	class TextTest : public Renderer2DTest
	{
	public:
		TextTest(Renderer2D* renderer)
			: Renderer2DTest(renderer)
		{}
		virtual ~TextTest() = default;

	public:
		virtual void Initialize() override;
		virtual void Cleanup() override;
		virtual void Update(float deltaTime) override;

	private:
		Font* m_ArialFont = nullptr;
	};

}
