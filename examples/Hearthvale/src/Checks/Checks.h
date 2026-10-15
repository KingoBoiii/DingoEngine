#pragma once
#include "Checks/CheckReport.h"

namespace Dingo
{

	class SuiteScope
	{
	public:
		SuiteScope(CheckReport& report, const char* name)
			: m_Report(report), m_Name(name), m_Passed(report.GetPassed()), m_Failed(report.GetFailed())
		{}

		~SuiteScope()
		{
			DE_INFO("{} checks: {} passed, {} failed", m_Name, m_Report.GetPassed() - m_Passed, m_Report.GetFailed() - m_Failed);
		}

	private:
		CheckReport& m_Report;
		const char* m_Name;
		int m_Passed;
		int m_Failed;
	};

	// The suites that need no window, device or asset: plain C++, safe under --graphics=headless.
	void RunDefChecks(CheckReport& report);
	void RunGenerationChecks(CheckReport& report);
	void RunCalendarChecks(CheckReport& report);
	void RunWorldChecks(CheckReport& report);
	void RunSkyChecks(CheckReport& report);
	void RunPlayerChecks(CheckReport& report);
	void RunSimChecks(CheckReport& report);

}
