#pragma once
#include "Tests/GraphicsTest.h"

namespace Dingo
{

	// TODO: Off-screen framebuffer targeting is a deferred feature.
	// These tests currently use the global swap-chain Renderer.
	class RendererTest : public GraphicsTest
	{
	public:
		RendererTest() = default;
		virtual ~RendererTest() = default;

	public:
		virtual void Resize(uint32_t width, uint32_t height) override;

		virtual Texture* GetResult() override { return Renderer::GetOutput(); }
	};

}
