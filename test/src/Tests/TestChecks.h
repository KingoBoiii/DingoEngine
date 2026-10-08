#pragma once
#include <DingoEngine.h>

#include <string>
#include <vector>

namespace Dingo
{

	struct CheckResult
	{
		std::string Name;
		bool Passed = false;
	};

	// A test's PASS/FAIL checks. Each is logged as it is made, "[PASS] name" or "[FAIL] name", which
	// is what scripted runs read, and kept for the test's Properties panel.
	class TestChecks
	{
	public:
		void Check(bool condition, const std::string& name)
		{
			m_Results.push_back({ name, condition });
			if (condition)
				DE_INFO("[PASS] {}", name);
			else
				DE_ERROR("[FAIL] {}", name);
		}

		void clear() { m_Results.clear(); }
		size_t size() const { return m_Results.size(); }
		bool empty() const { return m_Results.empty(); }
		std::vector<CheckResult>::const_iterator begin() const { return m_Results.begin(); }
		std::vector<CheckResult>::const_iterator end() const { return m_Results.end(); }

		int FailedCount() const
		{
			int failed = 0;
			for (const CheckResult& result : m_Results)
				failed += result.Passed ? 0 : 1;
			return failed;
		}

	private:
		std::vector<CheckResult> m_Results;
	};

}
