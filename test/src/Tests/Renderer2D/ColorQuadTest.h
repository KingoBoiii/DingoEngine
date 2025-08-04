#pragma once
#include "Renderer2DTest.h"

namespace Dingo
{

	class ColorQuadTest : public Renderer2DTest
	{
	public:
		ColorQuadTest(Renderer2D* renderer) 
			: Renderer2DTest(renderer)
		{}
		virtual ~ColorQuadTest() = default;

	public:
		virtual void Update(float deltaTime) override;
	};

}
