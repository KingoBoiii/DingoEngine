#pragma once
#include <DingoEngine.h>

namespace Dingo
{

	class TestLayer : public Layer
	{
	public:
		TestLayer() : Layer("Test Layer")
		{}
		virtual ~TestLayer() = default;

	public:
		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate() override;
		virtual void OnImGuiRender() override;

	private:
		CommandList* m_CommandList = nullptr;
	};

}

