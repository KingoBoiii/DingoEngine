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

		// Where the last rendered viewport image sits in the window (what Input::GetMousePosition
		// is measured in), and whether the pointer is over it.
		static const glm::vec2& GetImageOrigin() { return s_ImageOrigin; }
		static bool IsImageHovered() { return s_ImageHovered; }

	private:
		glm::vec2 m_ViewportSize = { 800.0f, 600.0f };
		static inline glm::vec2 s_ImageOrigin{ 0.0f };
		static inline bool s_ImageHovered = false;
	};

}
