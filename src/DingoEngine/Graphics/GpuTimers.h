#pragma once
#include "DingoEngine/Graphics/Renderer.h"

#include <memory>
#include <vector>

namespace Dingo::Internal
{

	// The renderer's GPU pass timers. Each frame's queries sit in one slot of a ring and are read
	// back when the slot comes round again, k_FrameSlots frames later, so a read never waits for the
	// GPU; a query that still isn't done by then is dropped. Every query a name gets in a frame adds
	// up to that frame's sample.
	class GpuTimers
	{
	public:
		static constexpr uint32_t k_FrameSlots = 4;
		static constexpr uint32_t k_MaxQueriesPerFrame = 32;
		static constexpr uint32_t k_HistoryLength = 120;

		GpuTimers();
		~GpuTimers();

		// Reads the slot the new frame reuses. Only while the render thread is parked: D3D11 reads
		// queries through the immediate context it presents with.
		void BeginFrame(uint64_t frameIndex);
		// Closes whatever the frame left open, before its command list closes.
		void EndFrame(CommandList* commandList);

		// A null list (a frame that renders nothing) opens nothing but keeps Begin and End paired.
		void Begin(CommandList* commandList, const char* name);
		void End(CommandList* commandList);

		const std::vector<GpuTimerStats>& GetStats() const;

	private:
		struct Data;
		std::unique_ptr<Data> m_Data;
	};

}
