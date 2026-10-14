#pragma once

// Engine-internal: a plain-data copy of the game UI context's last frame, for the F9 DingoUI tab.
// Keeps the context out of the ImGui code and ImGui out of the DingoUI code.

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Dingo::Internal::DingoUIDebug
{

	struct Stats
	{
		uint64_t Frame = 0;
		uint32_t Roots = 0;
		uint32_t Elements = 0;
		uint32_t Widgets = 0;
		uint32_t DrawCalls = 0;
		uint32_t Quads = 0;
		uint32_t CulledQuads = 0;
		uint32_t Vertices = 0;
		uint32_t Uploads = 0;
		uint32_t CacheHits = 0;
		uint64_t UploadBytes = 0;
		uint32_t StateMapSize = 0;
		uint32_t TweenCount = 0;
		uint32_t TransitionRoots = 0;
		uint32_t ExitingRoots = 0;
		uint32_t IdCollisions = 0;
		float BeginMs = 0.0f;
		float LayoutMs = 0.0f;
		float EmitMs = 0.0f;
		float HashMs = 0.0f;
		float BuildMs = 0.0f;
		float UploadMs = 0.0f;
		float DrawMs = 0.0f;
	};

	struct RootRow
	{
		uint64_t Id = 0;
		std::string Name;
		int Layer = 0;
		bool World = false;
		bool BlocksInput = true;
		bool Modal = false;
		bool Navigable = true;
		bool HasTransition = false;
		float TransitionProgress = 1.0f;
	};

	enum class ElementKind : uint8_t { Root, Box, Text, Image };

	struct ElementRow
	{
		uint64_t Id = 0;
		uint32_t Depth = 0;
		ElementKind Kind = ElementKind::Box;
		std::string Label;
		glm::vec2 Min{ 0.0f };
		glm::vec2 Max{ 0.0f };
		bool Interactive = false;
		bool Scroll = false;
		bool Focusable = false;
		bool BlocksInput = true;
		bool Hovered = false;
		bool Active = false;
		bool Focused = false;
	};

	struct Snapshot
	{
		Stats Frame;
		std::vector<RootRow> Roots;
		std::vector<ElementRow> Elements;
		std::vector<std::pair<glm::vec2, glm::vec2>> BlockingRects;
		uint64_t HoverId = 0;
		uint64_t ActiveId = 0;
		uint64_t FocusId = 0;
		bool FocusVisible = false;
		bool Modal = false;
		bool CapturingInput = false;
		bool PointerOverUI = false;
		glm::vec2 TargetSize{ 0.0f };
		// Multiply a point in target pixels by this to get it in window pixels (what Input reports).
		glm::vec2 TargetToWindow{ 1.0f };
		glm::vec2 Origin{ 0.0f }; // window pixels of target pixel (0, 0)
	};

	// False, and `out` untouched, while no DingoUI context exists (headless, or the UI turned off).
	// Elements and rects are filled only with `detail`.
	bool Collect(Snapshot& out, bool detail);

}
