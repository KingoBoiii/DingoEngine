#include "ClearColorTest.h"

#include <imgui.h>

namespace Dingo
{

	void ClearColorTest::Update(float deltaTime)
	{
		m_Renderer->Begin();
		m_Renderer->Clear(m_ClearColor);
		m_Renderer->End();
	}

	void ClearColorTest::ImGuiRender()
	{
		ImGui::ColorEdit4("Clear Color", (float*)&m_ClearColor);
	}

}
