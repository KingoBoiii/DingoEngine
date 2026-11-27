#pragma once

#include <functional>
#include <string>
#include <vector>

namespace Dingo
{

	class DustlightLayer;

	class Checks
	{
	public:
		void Tick(DustlightLayer& layer);
		bool IsDone() const { return m_Done; }

	private:
		struct Stage
		{
			std::function<void()> Enter;
			std::function<void()> Verify;
		};

		void Build(DustlightLayer& layer);
		void CheckSunCurve();
		void Check(bool passed, const std::string& name);
		void LogCost() const;
		void Finish();

	private:
		std::vector<Stage> m_Stages;
		size_t m_Index = 0;
		int m_Wait = -1;
		uint32_t m_StageBakes = 0;
		int m_ReadyFrames = 0;
		bool m_SunChecked = false;
		bool m_Built = false;
		bool m_Done = false;
		int m_Passed = 0;
		int m_Failed = 0;
	};

}
