#pragma once
#include <DingoEngine.h>

#include <string>
#include <vector>

namespace Dingo
{

	struct SuiteResult
	{
		std::string Name;
		int Passed = 0;
		int Failed = 0;
	};

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

		void AddSuite(const std::string& name, int passed, int failed)
		{
			for (SuiteResult& suite : m_Suites)
			{
				if (suite.Name == name)
				{
					suite.Passed += passed;
					suite.Failed += failed;
					return;
				}
			}
			m_Suites.push_back({ name, passed, failed });
		}

		const std::vector<SuiteResult>& GetSuites() const { return m_Suites; }

		void LogSummary() const
		{
			for (const SuiteResult& suite : m_Suites)
				DE_INFO("[Checks] {:<14} {:>4} passed, {} failed", suite.Name, suite.Passed, suite.Failed);
			DE_INFO("[Checks] {} passed, {} failed", m_Passed, m_Failed);
		}

	private:
		int m_Passed = 0;
		int m_Failed = 0;
		std::vector<SuiteResult> m_Suites;
	};

}
