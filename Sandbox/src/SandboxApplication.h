#pragma once
#include <DingoEngine.h>

class SandboxApplication : public DingoEngine::Application
{
public:
	SandboxApplication(const DingoEngine::ApplicationParams& params)
		: Application(params)
	{
	}
	virtual ~SandboxApplication() = default;

protected:
	virtual void OnInitialize() override;
	virtual void OnDestroy() override;
};
