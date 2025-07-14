#pragma once
#include "DingoEngine/Core/Layer.h"
#include "DingoEngine/ImGui/ImGuiParams.h"

#include "ImGuiRenderer.h"

namespace Dingo
{

	class ImGuiLayer : public Layer
	{
	public:
		ImGuiLayer(const ImGuiParams& params = {}) : Layer("ImGui Layer"), m_Params(params) {}
		virtual ~ImGuiLayer() = default;

	public:
		virtual void OnAttach() override;
		virtual void OnDetach() override;

		void Begin();
		void End();

	private:
		void SeupImGuiConfigFlags(ImGuiIO& io);
		void InitializePlatformInterface();

	private:
		ImGuiParams m_Params;
		ImGuiRenderer* m_ImGuiRenderer = nullptr;
	};

}
