#pragma once
#include "GraphicsTest.h"

#include <DingoEngine.h>

namespace Dingo
{
	class ClearColorTest : public GraphicsTest
	{
	public:
		ClearColorTest() = default;
		virtual ~ClearColorTest() = default;

	public:
		virtual void Update(float deltaTime) override;

		virtual void ImGuiRender() override;
		
	private:
		glm::vec3 m_ClearColor = glm::vec3(0.0f, 0.0f, 0.0f);
	};

} // namespace Dingo
