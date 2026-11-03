#include "Sim/PerfWindow.h"

#include <algorithm>

namespace Dingo
{

	const char* ToString(PerfMetric metric)
	{
		switch (metric)
		{
			case PerfMetric::Frame:     return "frame";
			case PerfMetric::Sim:       return "sim";
			case PerfMetric::Scene:     return "Scene::OnUpdate";
			case PerfMetric::Sync:      return "world sync";
			case PerfMetric::Ui:        return "DingoUI";
			case PerfMetric::Render:    return "render";
			case PerfMetric::DrawCalls: return "draw calls";
			default:                    return "?";
		}
	}

	PerfWindow::PerfWindow(int warmupFrames, int windowFrames)
		: m_Warmup((std::max)(0, warmupFrames)), m_Window((std::max)(1, windowFrames))
	{}

	void PerfWindow::Add(const PerfSample& sample)
	{
		if (IsDone())
			return;
		if (m_Seen < m_Warmup)
		{
			++m_Seen;
			return;
		}

		for (size_t i = 0; i < sample.size(); ++i)
		{
			m_Sum[i] += sample[i];
			m_Max[i] = (std::max)(m_Max[i], sample[i]);
		}
		++m_Counted;
	}

	float PerfWindow::Mean(PerfMetric metric) const
	{
		return m_Counted > 0 ? m_Sum[static_cast<size_t>(metric)] / static_cast<float>(m_Counted) : 0.0f;
	}

	float PerfWindow::Max(PerfMetric metric) const
	{
		return m_Max[static_cast<size_t>(metric)];
	}

}
