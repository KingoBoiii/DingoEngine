#pragma once
#include "Test.h"

#include <DingoEngine.h>

namespace Dingo
{
	class ClearColorTest : public Test
	{
	public:
		ClearColorTest() = default;
		virtual ~ClearColorTest() = default;

	public:
		virtual void Initialize() override;
		virtual void Update(float deltaTime) override;
		virtual void Cleanup() override;

	private:
		CommandList* m_CommandList = nullptr;
	};

} // namespace Dingo
