#pragma once
#include "DingoEngine/Core/Layer.h"
#include "DingoEngine/UI/UIParams.h"

#include "ImGuiRenderer.h"

namespace Dingo
{

	class ImGuiLayer : public Layer
	{
	public:
		ImGuiLayer(const UIParams& params = {}) : Layer("ImGui Layer"), m_Params(params) {}
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
		UIParams m_Params;
		ImGuiRenderer* m_ImGuiRenderer = nullptr;
	};

}
