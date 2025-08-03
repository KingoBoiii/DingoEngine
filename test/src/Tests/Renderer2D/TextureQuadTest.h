#pragma once
#include "Renderer2DTest.h"

namespace Dingo
{

	class TextureQuadTest : public Renderer2DTest
	{
	public:
		TextureQuadTest() = default;
		virtual ~TextureQuadTest() = default;

	public:
		virtual void Update(float deltaTime) override;
	};

}
