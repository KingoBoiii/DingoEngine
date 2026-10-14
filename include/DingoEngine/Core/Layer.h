#pragma once

#include "DingoEngine/Events/Event.h"

namespace Dingo
{

	/// Where game code lives: subclass it, override the hooks you need and hand it to
	/// Application::PushLayer or PushOverlay. The Application owns the layer from then on and
	/// deletes it at teardown. Layers update in push order, overlays after them; events go the
	/// other way, overlays first. All hooks run on the main thread.
	class Layer
	{
	public:
		/// name is a debug name only; the profiler shows it beside the layer's zones.
		Layer(const std::string& name);
		/// Deleted by the Application after OnDetach; never delete a pushed layer yourself.
		virtual ~Layer() = default;

	public:
		/// Called inside PushLayer or PushOverlay, once the layer is on the stack. Push from
		/// Application::OnInitialize, when the window, renderers, audio and assets are live.
		virtual void OnAttach() {}
		/// Called once when the Application shuts down, before the layer is deleted and while the
		/// renderer still answers queries. Free GPU resources here, not in the destructor.
		virtual void OnDetach() {}
		/// Called once per frame with the seconds since the previous frame. Not called while the
		/// app is paused in the background (see ApplicationParams::UpdateInBackground).
		virtual void OnUpdate(float deltaTime) {}
		/// Receives window, key, mouse and gamepad events. Set e.Handled to stop the event from
		/// reaching the layers below; EventDispatcher routes by type.
		virtual void OnEvent(Event& e) {}
		/// Draw immediate-mode UI (DingoEngine/UI/UI.h) here. EnableImGui must be enabled for this
		/// to be called.
		virtual void OnImGuiRender() {}

		/// The debug name given at construction.
		const std::string& GetName() const { return m_Name; }

	private:
		std::string m_Name;
	};

}
