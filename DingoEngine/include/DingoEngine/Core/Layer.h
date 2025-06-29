#pragma once

namespace DingoEngine
{

	class Layer
	{
	public:
		Layer(const std::string& name);
		virtual ~Layer() = default;

	public:
		virtual void OnAttach() {}
		virtual void OnDetach() {}
		virtual void OnUpdate() {}

	private:
		std::string m_Name;
	};

}
