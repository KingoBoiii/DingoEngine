#pragma once
#include <DingoEngine.h>

class SandboxApplication : public Dingo::Application
{
public:
	SandboxApplication(const Dingo::ApplicationParams& params)
		: Application(params)
	{
	}
	virtual ~SandboxApplication() = default;

protected:
	virtual void OnInitialize() override;
	virtual void OnDestroy() override;
};
