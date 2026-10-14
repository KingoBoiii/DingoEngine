#pragma once
#include <DingoEngine.h>

#include <glm/glm.hpp>

namespace Dingo
{

	class TestViewportPanel
	{
	public:
		TestViewportPanel() = default;
		~TestViewportPanel() = default;

	public:
		void OnImGuiRender(Texture* texture);

		const glm::vec2& GetViewportSize() const { return m_ViewportSize; }

	private:
		glm::vec2 m_ViewportSize = { 800.0f, 600.0f };
	};

}
