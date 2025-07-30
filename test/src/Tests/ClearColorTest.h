#pragma once
#include "Tests/Test.h"

#include <glm/glm.hpp>

namespace Dingo
{

	class ClearColorTest : public Test
	{
	public:
		ClearColorTest() = default;
		virtual ~ClearColorTest() = default;

	public:
		virtual void ImGuiRender() override;

	protected:
		glm::vec4 m_ClearColor = { 0.2f, 0.3f, 0.3f, 1.0f };
	};

}
