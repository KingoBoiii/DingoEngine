#include "ClearColorTest.h"

#include <imgui.h>

namespace Dingo
{

	void ClearColorTest::Update(float deltaTime)
	{
		m_CommandList->Begin();
		m_CommandList->Clear(m_Framebuffer, 0, m_ClearColor);
		m_CommandList->End();
	}

	void ClearColorTest::ImGuiRender()
	{
		ImGui::ColorEdit3("Clear Color", (float*)&m_ClearColor);
	}

}
