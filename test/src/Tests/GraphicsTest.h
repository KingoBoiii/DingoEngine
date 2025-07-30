#pragma once
#include "Tests/Test.h"

#include <glm/glm.hpp>

namespace Dingo
{

	class GraphicsTest : public Test
	{
	public:
		GraphicsTest() = default;
		virtual ~GraphicsTest() = default;

	public:
		virtual void ImGuiRender() override;

	protected:
		float m_AspectRatio = 1.0f;
		glm::vec4 m_ClearColor = { 0.2f, 0.3f, 0.3f, 1.0f };
	};

}
