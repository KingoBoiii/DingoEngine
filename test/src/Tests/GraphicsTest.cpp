#include "GraphicsTest.h"

#include <imgui.h>

namespace Dingo
{

	void GraphicsTest::ImGuiRender()
	{
		ImGui::Text("Aspect Ratio: %.2f", m_AspectRatio);
		ImGui::ColorEdit4("Clear Color", (float*)&m_ClearColor);
	}

}
