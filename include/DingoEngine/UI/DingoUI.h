#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <limits>
#include <string_view>

namespace Dingo
{

	class Font;
	class Texture;

	/// Options for the engine's game UI (DingoUI); set on ApplicationParams::DingoUI.
	struct DingoUIParams
	{
		/// Create the game UI. It costs nothing while no element is declared; off, every DingoUI
		/// call returns an empty result. A headless application never has one.
		bool Enabled = true;
		/// The font Text uses when its UITextParams names none. The game owns it and must keep it
		/// alive while it is set; DingoUI::SetDefaultFont changes it at run time, which is how a
		/// game that loads its font after the Application exists sets it. Text with no font at all
		/// warns once and is skipped.
		Font* DefaultFont = nullptr;
	};

	/// Where a root sits on the screen. The anchor is a point of the screen; the root's pivot (the
	/// same point of the root by default) is placed on it.
	enum class UIAnchor
	{
		TopLeft,
		Top,
		TopRight,
		Left,
		Center,
		Right,
		BottomLeft,
		Bottom,
		BottomRight,
		Stretch ///< The root fills the screen; its Width and Height are ignored.
	};

	/// The axis a container lays its children along.
	enum class UIDirection
	{
		Row,   ///< Left to right.
		Column ///< Top to bottom.
	};

	/// Where children sit across the container's direction.
	enum class UIAlign
	{
		Start,  ///< Top of a row, left of a column.
		Center,
		End,
		Stretch ///< Children that size to fit, or grow, fill the container across its direction.
	};

	/// Where the children sit along the container's direction when they leave room over.
	enum class UIJustify
	{
		Start,
		Center,
		End,
		SpaceBetween ///< The first child at the start, the last at the end, the rest spread evenly.
	};

	/// How a line of text sits in the width its element is given.
	enum class UITextAlign
	{
		Left,
		Center,
		Right
	};

	/// How an element's size on one axis is decided.
	enum class UISizeMode
	{
		Fit,     ///< As big as its content.
		Fixed,   ///< A number of pixels.
		Grow,    ///< A share, by weight, of what its siblings leave over along the container's direction; across it, the whole container.
		Percent  ///< A fraction of the parent's content area on that axis.
	};

	/// An element's size on one axis, with optional limits. All pixel values are in the root's
	/// design units: a root with a scale (UIScale) multiplies them.
	struct UISize
	{
		UISizeMode Mode = UISizeMode::Fit; ///< How the size is decided.
		float Value = 0.0f;                ///< Pixels for Fixed, the weight for Grow, the fraction (1 = all) for Percent.
		float MinSize = 0.0f;              ///< The size never goes below this many pixels.
		float MaxSize = (std::numeric_limits<float>::max)(); ///< The size never goes above this many pixels.

		/// As big as the content.
		static UISize Fit() { return UISize{}; }
		/// Exactly `pixels`.
		static UISize Fixed(float pixels) { return UISize{ UISizeMode::Fixed, pixels }; }
		/// A share of the leftover space; two elements of weight 1 and 3 split it one to three.
		static UISize Grow(float weight = 1.0f) { return UISize{ UISizeMode::Grow, weight }; }
		/// `fraction` of the parent's content area (padding excluded); 0.5 is half.
		static UISize Percent(float fraction) { return UISize{ UISizeMode::Percent, fraction }; }

		/// A copy that never goes below `pixels`.
		UISize SetMin(float pixels) const { UISize size = *this; size.MinSize = pixels; return size; }
		/// A copy that never goes above `pixels`.
		UISize SetMax(float pixels) const { UISize size = *this; size.MaxSize = pixels; return size; }
	};

	/// How a root turns design pixels into screen pixels.
	struct UIScale
	{
		/// What scales the root.
		enum class Kind
		{
			ConstantPixel,   ///< One design pixel is Factor screen pixels, at any resolution.
			ScaleWithScreen  ///< The root grows and shrinks with the screen, relative to a reference resolution.
		};

		Kind ScaleKind = Kind::ConstantPixel; ///< Which rule applies.
		float Factor = 1.0f;                  ///< ConstantPixel: the multiplier.
		glm::vec2 Reference{ 1920.0f, 1080.0f }; ///< ScaleWithScreen: the resolution the layout was designed for.
		/// ScaleWithScreen: 0 follows the width, 1 follows the height, in between blends them
		/// geometrically (the scale is 2^lerp(log2(width ratio), log2(height ratio), Match)).
		float Match = 0.5f;

		/// Design pixels are screen pixels times `factor`.
		static UIScale ConstantPixel(float factor = 1.0f)
		{
			UIScale scale;
			scale.Factor = factor;
			return scale;
		}
		/// Scale so the layout keeps its proportions as the screen leaves `reference`. `match`
		/// picks whether width (0) or height (1) decides when the aspect ratios differ.
		static UIScale ScaleWithScreen(const glm::vec2& reference = glm::vec2(1920.0f, 1080.0f), float match = 0.5f)
		{
			UIScale scale;
			scale.ScaleKind = Kind::ScaleWithScreen;
			scale.Reference = reference;
			scale.Match = match;
			return scale;
		}
	};

	/// The layout of a container: used by boxes and by roots. Chain the setters.
	struct UIBoxParams
	{
		/// Names the element, so it keeps its identity between frames. Only valid for the call it
		/// is passed to.
		std::string_view Id;
		UISize Width;                       ///< Size across the screen.
		UISize Height;                      ///< Size down the screen.
		UIDirection Direction = UIDirection::Column; ///< The axis children go along.
		float Gap = 0.0f;                   ///< Space between children, in design pixels.
		/// Space inside the edges, in design pixels: left, top, right, bottom.
		glm::vec4 Padding{ 0.0f };
		UIJustify Justify = UIJustify::Start; ///< Placement along Direction.
		UIAlign Align = UIAlign::Start;       ///< Placement across Direction.
		glm::vec4 Background{ 0.0f };       ///< Fill colour, straight alpha; fully transparent draws nothing.
		glm::vec4 BorderColor{ 0.0f };      ///< Border colour; drawn only with a BorderWidth above zero.
		float BorderWidth = 0.0f;           ///< Border thickness inside the edge, in design pixels.
		float CornerRadius = 0.0f;          ///< Rounds the corners of the fill and the border, in design pixels.
		bool ClipChildren = false;          ///< Draws nothing of the children outside this box.

		/// Sets Id.
		UIBoxParams& SetId(std::string_view id) { Id = id; return *this; }
		/// Sets Width and Height.
		UIBoxParams& SetSize(const UISize& width, const UISize& height) { Width = width; Height = height; return *this; }
		/// Sets Width.
		UIBoxParams& SetWidth(const UISize& width) { Width = width; return *this; }
		/// Sets Height.
		UIBoxParams& SetHeight(const UISize& height) { Height = height; return *this; }
		/// Sets Direction.
		UIBoxParams& SetDirection(UIDirection direction) { Direction = direction; return *this; }
		/// Sets Gap.
		UIBoxParams& SetGap(float gap) { Gap = gap; return *this; }
		/// The same padding on every side.
		UIBoxParams& SetPadding(float all) { Padding = glm::vec4(all); return *this; }
		/// `horizontal` on the left and right, `vertical` on the top and bottom.
		UIBoxParams& SetPadding(float horizontal, float vertical) { Padding = { horizontal, vertical, horizontal, vertical }; return *this; }
		/// Padding per side.
		UIBoxParams& SetPadding(float left, float top, float right, float bottom) { Padding = { left, top, right, bottom }; return *this; }
		/// Sets Justify.
		UIBoxParams& SetJustify(UIJustify justify) { Justify = justify; return *this; }
		/// Sets Align.
		UIBoxParams& SetAlign(UIAlign align) { Align = align; return *this; }
		/// Sets Background.
		UIBoxParams& SetBackground(const glm::vec4& color) { Background = color; return *this; }
		/// Sets BorderWidth and BorderColor.
		UIBoxParams& SetBorder(float width, const glm::vec4& color) { BorderWidth = width; BorderColor = color; return *this; }
		/// Sets CornerRadius.
		UIBoxParams& SetCornerRadius(float radius) { CornerRadius = radius; return *this; }
		/// Sets ClipChildren.
		UIBoxParams& SetClipChildren(bool clip) { ClipChildren = clip; return *this; }
	};

	/// A root of the UI: a container anchored to the screen. Chain the setters.
	struct UIRootParams
	{
		UIAnchor Anchor = UIAnchor::TopLeft; ///< The screen point the root is placed on.
		glm::vec2 Offset{ 0.0f };            ///< Moves the root from its anchor, in design pixels.
		/// Which point of the root sits on the anchor, as a fraction of its size (0, 0 top-left;
		/// 1, 1 bottom-right). Used only when HasPivot is set; otherwise it follows the anchor.
		glm::vec2 Pivot{ 0.0f };
		bool HasPivot = false;               ///< Use Pivot instead of the anchor's own point.
		UIScale Scale;                       ///< Design pixels to screen pixels for the whole root.
		int Layer = 0;                       ///< Roots draw in order of Layer, then of declaration.
		/// A press on one of its elements is taken from the game (the elements that draw something
		/// count; a bare layout box does not). Turn off for a HUD the pointer should pass through.
		bool BlocksInput = true;
		/// The root's own layout; its Width and Height are the root's size.
		UIBoxParams Layout;

		/// Sets Anchor.
		UIRootParams& SetAnchor(UIAnchor anchor) { Anchor = anchor; return *this; }
		/// Sets Offset.
		UIRootParams& SetOffset(const glm::vec2& offset) { Offset = offset; return *this; }
		/// Sets Pivot and turns HasPivot on.
		UIRootParams& SetPivot(const glm::vec2& pivot) { Pivot = pivot; HasPivot = true; return *this; }
		/// Sets Scale.
		UIRootParams& SetScale(const UIScale& scale) { Scale = scale; return *this; }
		/// Sets Layer.
		UIRootParams& SetLayer(int layer) { Layer = layer; return *this; }
		/// Sets BlocksInput.
		UIRootParams& SetBlocksInput(bool blocks) { BlocksInput = blocks; return *this; }
		/// Sets Layout.
		UIRootParams& SetLayout(const UIBoxParams& layout) { Layout = layout; return *this; }
	};

	/// A line or block of text. Chain the setters.
	struct UITextParams
	{
		float Size = 16.0f;                 ///< Height of a line, ascender to descender, in design pixels.
		glm::vec4 Color{ 1.0f };            ///< Text colour, straight alpha.
		UITextAlign Align = UITextAlign::Left; ///< Alignment of each line within the element's width.
		/// Break lines at the width the element is given (and at MaxWidth); off, the text stays on
		/// the lines the string has and never shrinks.
		bool Wrap = true;
		float MaxWidth = 0.0f;              ///< Wrap at this many design pixels at most; 0 for none.
		UISize Width;                       ///< Width of the element; Fit is the text's own width, shrunk to what the parent has when Wrap is on.
		const Font* TextFont = nullptr;     ///< The font; null uses the DefaultFont.

		/// Sets Size.
		UITextParams& SetSize(float size) { Size = size; return *this; }
		/// Sets Color.
		UITextParams& SetColor(const glm::vec4& color) { Color = color; return *this; }
		/// Sets Align.
		UITextParams& SetAlign(UITextAlign align) { Align = align; return *this; }
		/// Sets Wrap.
		UITextParams& SetWrap(bool wrap) { Wrap = wrap; return *this; }
		/// Sets MaxWidth.
		UITextParams& SetMaxWidth(float maxWidth) { MaxWidth = maxWidth; return *this; }
		/// Sets Width.
		UITextParams& SetWidth(const UISize& width) { Width = width; return *this; }
		/// Sets TextFont.
		UITextParams& SetFont(const Font* font) { TextFont = font; return *this; }
	};

	/// A picture. Chain the setters.
	struct UIImageParams
	{
		/// Width of the element; Fit is the texture's width in pixels.
		UISize Width;
		/// Height of the element; Fit is the texture's height in pixels (or, with PreserveAspect, whatever keeps the proportions at the width).
		UISize Height;
		/// The part of the texture drawn, as Renderer2D's UVs: (0, 0) is the bottom-left.
		glm::vec2 UVMin{ 0.0f };
		glm::vec2 UVMax{ 1.0f }; ///< The top-right of the part drawn.
		glm::vec4 Tint{ 1.0f };  ///< Multiplied into the texture; straight alpha.
		/// Draw the picture at its own proportions, centred in the element, instead of stretching it to fill.
		bool PreserveAspect = true;

		/// Sets Width and Height.
		UIImageParams& SetSize(const UISize& width, const UISize& height) { Width = width; Height = height; return *this; }
		/// Sets UVMin and UVMax.
		UIImageParams& SetUV(const glm::vec2& uvMin, const glm::vec2& uvMax) { UVMin = uvMin; UVMax = uvMax; return *this; }
		/// Sets Tint.
		UIImageParams& SetTint(const glm::vec4& tint) { Tint = tint; return *this; }
		/// Sets PreserveAspect.
		UIImageParams& SetPreserveAspect(bool preserve) { PreserveAspect = preserve; return *this; }
	};

	/// The game UI: HUDs and menus declared every frame from OnUpdate, laid out, drawn over the
	/// scene and its post chain, and taking the pointer from the game where it lands on them.
	/// Static, like Input; call it from the main thread. ImGui stays the debug UI (Dingo::UI).
	///
	/// A frame declares roots, each holding boxes, text and images; the engine lays them out after
	/// the last OnUpdate and draws them in one pass. Every Begin must be matched by its End, and
	/// the End is called only when the Begin returned true:
	/// @code
	/// if (DingoUI::BeginRoot("hud", UIRootParams().SetAnchor(UIAnchor::TopLeft).SetOffset({ 16, 16 })))
	/// {
	///     DingoUI::Text("Score 120");
	///     DingoUI::EndRoot();
	/// }
	/// @endcode
	class DingoUI
	{
	public:
		/// DingoUI is never instantiated; everything is static.
		DingoUI() = delete;

		/// Opens a root named `name`. Returns false, and the End must not be called, when there is
		/// no game UI (headless, disabled) or the call is outside a frame's OnUpdate.
		static bool BeginRoot(std::string_view name, const UIRootParams& params = {});
		/// Closes the root opened by the matching BeginRoot.
		static void EndRoot();
		/// Opens a container inside the current root or box. Returns false, and EndBox must not be
		/// called, when no root is open.
		static bool BeginBox(const UIBoxParams& params = {});
		/// Closes the box opened by the matching BeginBox.
		static void EndBox();
		/// Adds text to the current container. Skipped, with one warning, when neither `params`
		/// nor DingoUIParams::DefaultFont names a font. `text` is UTF-8; it is copied.
		static void Text(std::string_view text, const UITextParams& params = {});
		/// Adds a picture to the current container. A null texture draws the tint alone, with no
		/// size of its own.
		static void Image(Texture* texture, const UIImageParams& params = {});

		/// Sets the font Text uses when its params name none. Pass nullptr before deleting the font.
		static void SetDefaultFont(Font* font);
		/// The font set by DingoUIParams::DefaultFont or SetDefaultFont, or nullptr.
		static Font* GetDefaultFont();

		/// True while the pointer is over an element that blocks input (as of the last frame's
		/// layout), for a game that reads the raw mouse and wants to ignore clicks on the UI.
		static bool IsPointerOverUI();
		/// True while the UI holds the pointer or the keyboard (a press it took and has not
		/// released, or a modal), so the game should leave that input alone.
		static bool IsCapturingInput();
	};

}
