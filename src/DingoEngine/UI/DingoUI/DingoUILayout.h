#pragma once
#include "DingoEngine/UI/DingoUI.h"
#include "DingoUIDrawList.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace Dingo::Internal
{

	enum class UIElementKind : uint8_t
	{
		Root,
		Box,
		Text,
		Image
	};

	// One size on one axis in pixels (the root's scale already applied).
	struct UIAxisSize
	{
		UISizeMode Mode = UISizeMode::Fit;
		float Value = 0.0f;
		float Min = 0.0f;
		float Max = (std::numeric_limits<float>::max)();
	};

	inline constexpr int32_t k_UINoElement = -1;

	// An element of one frame's tree. Elements live in declaration order in one vector (a parent
	// always before its children), so layout walks it forwards and backwards with no recursion.
	// Id is the hash of the id path and stays stable between frames while the declaration order
	// does, which is what the interaction state of a later phase keys on.
	struct UIElement
	{
		UIElementKind Kind = UIElementKind::Box;
		int32_t Parent = k_UINoElement;
		int32_t FirstChild = k_UINoElement;
		int32_t LastChild = k_UINoElement;
		int32_t NextSibling = k_UINoElement;
		uint32_t ChildCount = 0;

		uint64_t Id = 0;
		uint64_t NameHash = 0; // of the name the game gave, 0 for none

		std::array<UIAxisSize, 2> Size; // x, y
		bool Horizontal = false;        // children go along x
		float Gap = 0.0f;
		glm::vec4 Padding{ 0.0f };      // left, top, right, bottom
		UIJustify Justify = UIJustify::Start;
		UIAlign Align = UIAlign::Start;
		glm::vec4 Background{ 0.0f };
		glm::vec4 BorderColor{ 0.0f };
		float BorderWidth = 0.0f;
		float CornerRadius = 0.0f;
		bool ClipChildren = false;

		// Root
		UIAnchor Anchor = UIAnchor::TopLeft;
		glm::vec2 Offset{ 0.0f };
		glm::vec2 Pivot{ 0.0f };
		int Layer = 0;
		bool BlocksInput = true;
		float Scale = 1.0f;

		// Text
		const Font* TextFont = nullptr;
		uint32_t TextOffset = 0;
		uint32_t TextLength = 0;
		float TextSize = 0.0f;
		glm::vec4 TextColor{ 1.0f };
		UITextAlign TextAlign = UITextAlign::Left;
		bool Wrap = false;
		float MaxWidth = 0.0f;
		float WrapWidth = 0.0f;       // the maxWidth the text was last measured with, also what it draws with
		float MinContentWidth = -1.0f; // widest word; -1 until needed

		// Image
		Texture* ImageTexture = nullptr;
		glm::vec2 ImageSize{ 0.0f };  // pixels, scaled
		glm::vec2 UVMin{ 0.0f };
		glm::vec2 UVMax{ 1.0f };
		glm::vec4 Tint{ 1.0f };
		bool PreserveAspect = true;

		// Layout scratch and result
		std::array<float, 2> Natural{};  // content size, padding included
		std::array<float, 2> Resolved{}; // final size
		glm::vec2 Origin{ 0.0f };        // top-left before pixel snapping
		UIRect Rect;                     // final, snapped to whole pixels
		UIRect Clip = k_UINoClip;        // what the element's own drawing is clipped to
	};

	// Measures `text` at `size` pixels, wrapping at `maxWidth` when above zero: width of the widest
	// line and height of all of them. The layout never touches a font itself; the context passes
	// one that asks Font::MeasureText, a test passes a fixed-advance one.
	using UITextMeasureFn = std::function<glm::vec2(const Font* font, std::string_view text, float size, float maxWidth)>;

	// The per-frame element tree and the layout that sizes it. GPU-free.
	class UILayoutTree
	{
	public:
		// Starts a frame on a target of `viewport` pixels. Keeps the memory of the last one.
		void Begin(const glm::vec2& viewport);

		// False (nothing added) when a root is already open, or no root is open for the others.
		bool BeginRoot(std::string_view name, const UIRootParams& params);
		bool BeginBox(const UIBoxParams& params);
		bool AddText(std::string_view text, const UITextParams& params, const Font* font);
		// naturalSize is the texture's size in pixels; the texture is never dereferenced.
		bool AddImage(Texture* texture, const glm::vec2& naturalSize, const UIImageParams& params);
		// False when the open element isn't a root / box.
		bool EndRoot();
		bool EndBox();
		// Closes whatever is still open; returns how many it closed.
		uint32_t CloseAll();
		uint32_t GetOpenCount() const { return static_cast<uint32_t>(m_Open.size()); }

		// Sizes and positions every element, into whole pixels.
		void Layout(const UITextMeasureFn& measure);
		// Appends the tree's drawing to `list`, roots by layer then declaration order.
		void Emit(UIDrawList& list) const;

		const glm::vec2& GetViewport() const { return m_Viewport; }
		const std::vector<UIElement>& GetElements() const { return m_Elements; }
		std::string_view GetText(const UIElement& element) const { return std::string_view(m_Text).substr(element.TextOffset, element.TextLength); }
		// The first element whose name (UIBoxParams::Id or a root's name) is `name`, or nullptr.
		const UIElement* FindByName(std::string_view name) const;
		// Rects of the elements of input-blocking roots that draw something.
		void CollectBlockingRects(std::vector<UIRect>& rects) const;

		static uint64_t HashName(std::string_view name);

	private:
		int32_t Append(UIElementKind kind);
		void ApplyBox(UIElement& element, const UIBoxParams& params);
		void FitAxis(uint32_t axis);
		void ResolveAxis(uint32_t axis);
		void MeasureText(UIElement& element, float maxWidth);
		void ComputeMinContent(UIElement& element);
		void Place();
		float ContentOf(const UIElement& child, uint32_t axis) const;
		static float Clamp(const UIAxisSize& size, float value);

	private:
		std::vector<UIElement> m_Elements;
		std::vector<int32_t> m_Open;
		mutable std::vector<int32_t> m_Order; // scratch: roots by layer
		std::string m_Text;
		glm::vec2 m_Viewport{ 0.0f };
		float m_RootScale = 1.0f;
		const UITextMeasureFn* m_Measure = nullptr;
	};

	// The factor a UIScale applies on a viewport of `viewport` pixels.
	float ComputeUIScale(const UIScale& scale, const glm::vec2& viewport);

}
