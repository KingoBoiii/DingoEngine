#pragma once
#include "DingoEngine/Core/Layer.h"
#include "DingoEngine/UI/ImGuiParams.h"

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
		virtual void OnEvent(Event& e) override;

		void Begin();
		void End();

		// As of ImGui's last NewFrame, so one frame behind the input they gate.
		bool WantsMouse() const;
		bool WantsKeyboard() const;
		bool BlocksGameInput() const { return m_Params.BlockGameInput; }

	private:
		void SeupImGuiConfigFlags(ImGuiIO& io);
		void InitializePlatformInterface();

	private:
		ImGuiParams m_Params;
		ImGuiRenderer* m_ImGuiRenderer = nullptr;
	};

}
