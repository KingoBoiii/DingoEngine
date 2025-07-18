#pragma once
#include <DingoEngine.h>

namespace Dingo
{

	class TestFrameworkApplication : public Application
	{
	public:
		TestFrameworkApplication(const Dingo::ApplicationParams& params)
			: Application(params)
		{}
		virtual ~TestFrameworkApplication() = default;

	protected:
		virtual void OnInitialize() override;
		virtual void OnDestroy() override;
	};

}
