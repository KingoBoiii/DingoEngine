#pragma once

// Engine-internal: the skinned instances a Renderer3D drew, into the view or a shadow, keyed by the
// palette they were submitted with, for this frame and the one before. A scene's animation pass reads
// it to leave the pose of a culled animator unevaluated (AnimatorComponent::CullingMode). Main thread
// only. Shared, so a scene holding it weakly outlives a freed renderer safely.
//
// Keyed by address, against the ids-not-addresses rule, because no lookup here can return a wrong
// object: a reused address makes a new animator count as seen for at most two frames (it animates),
// and a palette reallocated between render and update counts as culled for one update, after
// EnsureAnimator has already evaluated it.

#include <glm/glm.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <vector>

namespace Dingo::Internal
{

	class SkinnedVisibility
	{
	public:
		// A scene of `frame` ended: its slot starts empty on the frame's first scene.
		void BeginScene(uint64_t frame)
		{
			Slot& slot = m_Slots[frame & 1];
			if (slot.Frame == frame)
				return;
			slot.Frame = frame;
			slot.Palettes.clear();
			slot.Sorted = true;
		}

		void Record(uint64_t frame, const glm::mat4* palette)
		{
			Slot& slot = m_Slots[frame & 1];
			slot.Sorted = slot.Sorted && (slot.Palettes.empty() || std::less<>()(slot.Palettes.back(), palette));
			slot.Palettes.push_back(palette);
		}

		// False also when the renderer drew no scene in either frame.
		bool WasDrawn(uint64_t frame, const glm::mat4* palette) const
		{
			for (Slot& slot : m_Slots)
			{
				if (slot.Frame != frame && slot.Frame + 1 != frame)
					continue;
				if (!slot.Sorted)
				{
					std::sort(slot.Palettes.begin(), slot.Palettes.end(), std::less<>());
					slot.Sorted = true;
				}
				if (std::binary_search(slot.Palettes.begin(), slot.Palettes.end(), palette, std::less<>()))
					return true;
			}
			return false;
		}

	private:
		// A vector keeps its capacity across frames, so recording allocates nothing once warm.
		struct Slot
		{
			uint64_t Frame = ~0ull;
			std::vector<const glm::mat4*> Palettes;
			bool Sorted = true;
		};
		mutable Slot m_Slots[2];
	};

}
