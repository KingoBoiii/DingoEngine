#pragma once
#include <DingoEngine.h>

#include <string>

namespace Dingo
{

	class CheckReport
	{
	public:
		void Check(bool passed, const std::string& name)
		{
			if (passed)
			{
				++m_Passed;
				DE_INFO("[PASS] {}", name);
			}
			else
			{
				++m_Failed;
				DE_ERROR("[FAIL] {}", name);
			}
		}

		int GetPassed() const { return m_Passed; }
		int GetFailed() const { return m_Failed; }

	private:
		int m_Passed = 0;
		int m_Failed = 0;
	};

}
