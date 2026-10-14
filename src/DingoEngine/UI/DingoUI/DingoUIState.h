#pragma once
#include "DingoUIDrawList.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <unordered_map>

namespace Dingo::Internal
{

	// What the UI remembers about one widget between frames.
	struct UIWidgetState
	{
		bool Hovered = false;
		bool Active = false;
		bool Focused = false;           // set by keyboard and gamepad focus navigation
		UIRect LastRect;                // target pixels, from the layout of the last frame it was declared
		glm::vec2 ContentSize{ 0.0f };  // scroll areas: extent of the children, padding included
		glm::vec2 ScrollOffset{ 0.0f }; // scroll areas: pixels the content is moved up and left
		glm::vec2 PointerLast{ 0.0f };  // scroll drag: pointer position of the last frame
		// Eased 0..1 progress of the look, stepped by the style's TransitionDuration: hover, pressed,
		// focus ring, switched on (Toggle on, Selectable selected). AnimInit is set once they hold their first targets.
		std::array<float, 4> Anim{};
		bool AnimInit = false;
		uint64_t LastSeenFrame = 0;
	};

	// Id-keyed widget state. An entry that no frame has declared for more than `maxAge` frames is
	// dropped, so the map never outgrows the widgets in use.
	class UIStateMap
	{
	public:
		// An id undeclared for this many frames in a row loses its state.
		static constexpr uint64_t k_DefaultMaxAge = 2;

		struct TouchResult
		{
			UIWidgetState* State = nullptr;
			bool IsNew = false;
			bool DeclaredAlready = false; // a second declaration of the id in the same frame
		};

		TouchResult Touch(uint64_t id, uint64_t frame);
		UIWidgetState* Find(uint64_t id);
		const UIWidgetState* Find(uint64_t id) const;
		// Drops what was last seen `maxAge` or more frames before `frame` (called at the end of frame
		// `frame`, so it has gone undeclared for that many frames); returns how many.
		size_t Evict(uint64_t frame, uint64_t maxAge = k_DefaultMaxAge);
		void Clear() { m_States.clear(); }
		size_t Size() const { return m_States.size(); }

	private:
		std::unordered_map<uint64_t, UIWidgetState> m_States;
	};

}
