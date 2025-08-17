#pragma once
#include "Renderer2DTest.h"

namespace Dingo
{

	class CircleTest : public Renderer2DTest
	{
	public:
		CircleTest(Renderer2D* renderer)
			: Renderer2DTest(renderer)
		{}
		virtual ~CircleTest() = default;

	public:
		virtual void Update(float deltaTime) override;
	};

}
