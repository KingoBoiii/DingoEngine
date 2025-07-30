#pragma once
#include "Renderer2DTest.h"

namespace Dingo
{

	class ColorQuadTest : public Renderer2DTest
	{
	public:
		ColorQuadTest() = default;
		virtual ~ColorQuadTest() = default;

	public:
		virtual void Update(float deltaTime) override;
	};

}
