#pragma once
#include <array>
#include <cstddef>

namespace Dingo
{

	enum class PerfMetric : size_t { Frame, Sim, Scene, Sync, Ui, Render, DrawCalls, Count };

	const char* ToString(PerfMetric metric);

	using PerfSample = std::array<float, static_cast<size_t>(PerfMetric::Count)>;

	// Means and maxima of the frames after a warm-up, for `--perf`. It reads no clock: the layer times a frame and
	// hands it in, so the window can be checked with made-up frames.
	class PerfWindow
	{
	public:
		PerfWindow(int warmupFrames, int windowFrames);

		void Add(const PerfSample& sample);

		bool IsDone() const { return m_Counted >= m_Window; }
		int GetCounted() const { return m_Counted; }
		int GetWarmup() const { return m_Warmup; }
		float Mean(PerfMetric metric) const;
		float Max(PerfMetric metric) const;

	private:
		int m_Warmup;
		int m_Window;
		int m_Seen = 0;
		int m_Counted = 0;
		PerfSample m_Sum{};
		PerfSample m_Max{};
	};

}
