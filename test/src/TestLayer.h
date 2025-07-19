#pragma once
#include <DingoEngine.h>

#include "Tests/Test.h"

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
		Test* m_CurrentTest = nullptr;
		CommandList* m_CommandList = nullptr;
	};

}

