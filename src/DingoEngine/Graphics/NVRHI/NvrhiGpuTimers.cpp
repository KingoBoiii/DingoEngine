#include "depch.h"
#include "DingoEngine/Graphics/GpuTimers.h"
#include "DingoEngine/Core/Profiler.h"

#include "NvrhiCommandList.h"
#include "NvrhiGraphicsContext.h"

#include <algorithm>
#include <deque>

namespace Dingo::Internal
{

	namespace
	{
		nvrhi::IDevice* GetDevice()
		{
			return GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle();
		}

		constexpr uint32_t k_NoQuery = ~0u;
	}

	struct GpuTimers::Data
	{
		struct Query
		{
			nvrhi::TimerQueryHandle Handle;
			uint32_t Timer = 0;
			bool Ended = false;
		};

		struct Frame
		{
			std::vector<Query> Queries;
			uint32_t Used = 0;
		};

		// A deque, so the names Tracy and GpuTimerStats point into never move.
		struct Timer
		{
			std::string Name;
			std::string PlotName;
			uint32_t Depth = 0;
			float History[k_HistoryLength] = {};
			uint32_t Count = 0;
			uint32_t Cursor = 0;
		};

		Frame Frames[k_FrameSlots];
		uint32_t CurrentFrame = 0;
		std::vector<uint32_t> Open;
		std::deque<Timer> Timers;
		std::vector<GpuTimerStats> Stats;
		bool OverflowWarned = false;

		uint32_t FindOrAddTimer(const char* name, uint32_t depth)
		{
			for (uint32_t i = 0; i < Timers.size(); ++i)
			{
				if (Timers[i].Name == name)
					return i;
			}

			Timer& timer = Timers.emplace_back();
			timer.Name = name;
			timer.PlotName = std::string("GPU ") + name + " (ms)";
			timer.Depth = depth;
			return static_cast<uint32_t>(Timers.size() - 1);
		}
	};

	GpuTimers::GpuTimers()
		: m_Data(std::make_unique<Data>())
	{
	}

	GpuTimers::~GpuTimers() = default;

	void GpuTimers::BeginFrame(uint64_t frameIndex)
	{
		Data& data = *m_Data;
		data.CurrentFrame = static_cast<uint32_t>(frameIndex % k_FrameSlots);
		data.Open.clear();

		Data::Frame& frame = data.Frames[data.CurrentFrame];
		if (frame.Used == 0)
			return;

		nvrhi::IDevice* device = GetDevice();
		std::vector<float> sums(data.Timers.size(), 0.0f);
		std::vector<bool> measured(data.Timers.size(), false);
		for (uint32_t i = 0; i < frame.Used; ++i)
		{
			Data::Query& query = frame.Queries[i];
			if (query.Ended && device->pollTimerQuery(query.Handle))
			{
				sums[query.Timer] += device->getTimerQueryTime(query.Handle) * 1000.0f;
				measured[query.Timer] = true;
			}
			device->resetTimerQuery(query.Handle);
			query.Ended = false;
		}
		frame.Used = 0;

		for (uint32_t i = 0; i < data.Timers.size(); ++i)
		{
			if (!measured[i])
				continue;

			Data::Timer& timer = data.Timers[i];
			timer.History[timer.Cursor] = sums[i];
			timer.Cursor = (timer.Cursor + 1) % k_HistoryLength;
			timer.Count = std::min(timer.Count + 1, k_HistoryLength);
			Profiler::Plot(timer.PlotName.c_str(), sums[i]);
		}

		data.Stats.resize(data.Timers.size());
		for (uint32_t i = 0; i < data.Timers.size(); ++i)
		{
			const Data::Timer& timer = data.Timers[i];
			GpuTimerStats& stats = data.Stats[i];
			stats.Name = timer.Name.c_str();
			stats.Depth = timer.Depth;
			stats.Samples = timer.Count;
			if (timer.Count == 0)
				continue;

			float sum = 0.0f, max = 0.0f;
			for (uint32_t sample = 0; sample < timer.Count; ++sample)
			{
				sum += timer.History[sample];
				max = std::max(max, timer.History[sample]);
			}
			stats.LastMs = timer.History[(timer.Cursor + k_HistoryLength - 1) % k_HistoryLength];
			stats.MeanMs = sum / static_cast<float>(timer.Count);
			stats.MaxMs = max;
		}
	}

	void GpuTimers::EndFrame(CommandList* commandList)
	{
		while (!m_Data->Open.empty())
			End(commandList);
	}

	void GpuTimers::Begin(CommandList* commandList, const char* name)
	{
		Data& data = *m_Data;
		Data::Frame& frame = data.Frames[data.CurrentFrame];
		if (!commandList || !name)
		{
			data.Open.push_back(k_NoQuery);
			return;
		}

		if (frame.Used >= k_MaxQueriesPerFrame)
		{
			if (!data.OverflowWarned)
			{
				DE_CORE_WARN("Renderer: more than {} GPU timers in one frame; '{}' and later ones are not measured.", k_MaxQueriesPerFrame, name);
				data.OverflowWarned = true;
			}
			data.Open.push_back(k_NoQuery);
			return;
		}

		if (frame.Used == frame.Queries.size())
		{
			nvrhi::TimerQueryHandle handle = GetDevice()->createTimerQuery();
			if (!handle)
			{
				data.Open.push_back(k_NoQuery);
				return;
			}
			frame.Queries.push_back({ handle });
		}

		Data::Query& query = frame.Queries[frame.Used];
		query.Timer = data.FindOrAddTimer(name, static_cast<uint32_t>(data.Open.size()));
		query.Ended = false;
		static_cast<NvrhiCommandList*>(commandList)->GetNvrhiHandle()->beginTimerQuery(query.Handle);
		data.Open.push_back(frame.Used);
		++frame.Used;
	}

	void GpuTimers::End(CommandList* commandList)
	{
		Data& data = *m_Data;
		if (data.Open.empty())
			return;

		const uint32_t index = data.Open.back();
		data.Open.pop_back();
		if (index == k_NoQuery || !commandList)
			return;

		Data::Query& query = data.Frames[data.CurrentFrame].Queries[index];
		static_cast<NvrhiCommandList*>(commandList)->GetNvrhiHandle()->endTimerQuery(query.Handle);
		query.Ended = true;
	}

	const std::vector<GpuTimerStats>& GpuTimers::GetStats() const
	{
		return m_Data->Stats;
	}

}
