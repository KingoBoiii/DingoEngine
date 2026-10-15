#include "TestViewportPanel.h"

#include <imgui.h>

namespace Dingo
{

	void TestViewportPanel::OnImGuiRender(Texture* texture)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

		// Viewport Panel
		ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

		auto viewport = ImGui::GetContentRegionAvail();
		m_ViewportSize = glm::vec2(viewport.x, viewport.y);

		ImGui::Image(texture->GetTextureHandle(), viewport);
		const ImVec2 origin = ImGui::GetItemRectMin();
		s_ImageOrigin = glm::vec2(origin.x, origin.y);
		s_ImageHovered = ImGui::IsItemHovered();

		ImGui::End();

		ImGui::PopStyleVar();
	}

}
