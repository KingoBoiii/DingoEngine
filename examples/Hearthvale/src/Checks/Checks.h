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
			const int passed = m_Report.GetPassed() - m_Passed;
			const int failed = m_Report.GetFailed() - m_Failed;
			m_Report.AddSuite(m_Name, passed, failed);
			DE_INFO("{} checks: {} passed, {} failed", m_Name, passed, failed);
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
	void RunPlacementChecks(CheckReport& report);
	void RunGrowthChecks(CheckReport& report);
	void RunEconomyChecks(CheckReport& report);
	void RunBoardChecks(CheckReport& report);
	void RunUiLayoutChecks(CheckReport& report);
	void RunMenuChecks(CheckReport& report);
	void RunLifeChecks(CheckReport& report);
	void RunLookChecks(CheckReport& report);
	void RunSaveChecks(CheckReport& report);
	void RunAssetChecks(CheckReport& report);
	void RunAutoplayChecks(CheckReport& report);
	void RunDeterminismChecks(CheckReport& report);
	void RunSimChecks(CheckReport& report);

}
