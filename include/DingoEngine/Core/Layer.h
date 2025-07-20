#pragma once

namespace Dingo
{

	class Layer
	{
	public:
		Layer(const std::string& name);
		virtual ~Layer() = default;

	public:
		virtual void OnAttach() {}
		virtual void OnDetach() {}
		virtual void OnUpdate(float deltaTime) {}
		virtual void OnImGuiRender() {} // EnableImGui must be enabled for this to be called

	private:
		std::string m_Name;
	};

}
