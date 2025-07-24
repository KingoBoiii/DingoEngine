#pragma once
#include <DingoEngine.h>

#include "UI/TestViewportPanel.h"

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
		virtual void OnUpdate(float deltaTime) override;
		virtual void OnImGuiRender() override;

	private:
		TestViewportPanel m_TestViewportPanel;

		std::vector<std::pair<std::string, std::function<Test*()>>> m_Tests;
		Test* m_CurrentTest = nullptr;
	};

}

