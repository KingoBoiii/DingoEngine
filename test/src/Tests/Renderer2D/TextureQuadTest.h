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
		virtual void Initialize() override;
		virtual void Cleanup() override;
		virtual void Update(float deltaTime) override;

	private:
		Texture* m_Texture = nullptr;
	};

}
