#include "ClearColorTest.h"

#include <imgui.h>

namespace Dingo
{

	void ClearColorTest::ImGuiRender()
	{
		ImGui::ColorEdit4("Clear Color", (float*)&m_ClearColor);
	}

}
