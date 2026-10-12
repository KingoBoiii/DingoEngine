#pragma once

// Engine-internal: the skinned instances a Renderer3D drew, into the view or a shadow, keyed by the
// palette they were submitted with, for this frame and the one before. A scene's animation pass reads
// it to leave the pose of a culled animator unevaluated (AnimatorComponent::CullingMode). Main thread
// only. Shared, so a scene holding it weakly outlives a freed renderer safely.

#include <glm/glm.hpp>

#include <cstdint>
#include <unordered_set>

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
		}

		void Record(uint64_t frame, const glm::mat4* palette)
		{
			m_Slots[frame & 1].Palettes.insert(palette);
		}

		// False also when the renderer drew no scene in either frame.
		bool WasDrawn(uint64_t frame, const glm::mat4* palette) const
		{
			for (const Slot& slot : m_Slots)
			{
				if ((slot.Frame == frame || slot.Frame + 1 == frame) && slot.Palettes.contains(palette))
					return true;
			}
			return false;
		}

	private:
		struct Slot
		{
			uint64_t Frame = ~0ull;
			std::unordered_set<const glm::mat4*> Palettes;
		};
		Slot m_Slots[2];
	};

}
