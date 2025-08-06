#include "Renderer2DTest.h"

#include <glm/gtc/matrix_transform.hpp>

#include <imgui.h>

namespace Dingo
{

	void Renderer2DTest::Initialize()
	{
		RecalculateProjectionViewMatrix();
	}

	void Renderer2DTest::Cleanup()
	{
	}

	void Renderer2DTest::Resize(uint32_t width, uint32_t height)
	{
		if (width == m_Renderer->GetViewportSize().x && height == m_Renderer->GetViewportSize().y)
		{
			return;
		}

		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);

		RecalculateProjectionViewMatrix();
	}

	void Renderer2DTest::ImGuiRender()
	{
		GraphicsTest::ImGuiRender();

		if (ImGui::DragFloat("Orthographic Size", &m_OrthographicSize, 0.01f, 0.1f, 10.0f))
		{
			RecalculateProjectionViewMatrix();
		}

		if (ImGui::DragFloat("Orthographic Near", &m_OrthographicNear, 0.01f, -10.0f, 0.0f))
		{
			RecalculateProjectionViewMatrix();
		}

		if (ImGui::DragFloat("Orthographic Far", &m_OrthographicFar, 0.01f, 0.0f, 10.0f))
		{
			RecalculateProjectionViewMatrix();
		}
	}

	void Renderer2DTest::RecalculateProjectionViewMatrix()
	{
		float orthoLeft = -m_OrthographicSize * m_AspectRatio * 0.5f;
		float orthoRight = m_OrthographicSize * m_AspectRatio * 0.5f;
		float orthoBottom = -m_OrthographicSize * 0.5f;
		float orthoTop = m_OrthographicSize * 0.5f;

		m_ProjectionViewMatrix = glm::ortho(orthoLeft, orthoRight, orthoBottom, orthoTop, m_OrthographicNear, m_OrthographicFar);
	}

}
