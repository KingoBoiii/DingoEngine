#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <limits>
#include <string_view>

namespace Dingo
{

	class Font;
	class Texture;

	/// A picture cut into nine parts by four borders, so a panel or button face keeps its corners
	/// crisp at any size: the corners are drawn at the border size, the edges stretch along one
	/// axis, the centre along both. Chain the setters.
	struct UINineSlice
	{
		/// The picture; null means no 9-slice. The caller owns it and keeps it alive while it is used.
		Texture* Image = nullptr;
		/// The border widths in texture pixels: left, top, right, bottom.
		glm::vec4 Borders{ 0.0f };
		/// Drawn border size is Borders times Scale (and times the root's scale), rounded to whole pixels.
		float Scale = 1.0f;
		/// Draw the centre; off leaves a frame with a hole.
		bool FillCenter = true;
		/// Sample with the point (nearest) filter instead of the linear one, for pixel art.
		bool PointFilter = false;
		/// Multiplied into the picture; straight alpha.
		glm::vec4 Tint{ 1.0f };

		/// True when there is a picture to draw.
		bool IsValid() const { return Image != nullptr; }

		/// Sets Image and Borders.
		UINineSlice& SetImage(Texture* image, const glm::vec4& borders) { Image = image; Borders = borders; return *this; }
		/// The same border on every side.
		UINineSlice& SetBorders(float all) { Borders = glm::vec4(all); return *this; }
		/// Sets Borders: left, top, right, bottom.
		UINineSlice& SetBorders(float left, float top, float right, float bottom) { Borders = { left, top, right, bottom }; return *this; }
		/// Sets Scale.
		UINineSlice& SetScale(float scale) { Scale = scale; return *this; }
		/// Sets FillCenter.
		UINineSlice& SetFillCenter(bool fill) { FillCenter = fill; return *this; }
		/// Sets PointFilter.
		UINineSlice& SetPointFilter(bool point) { PointFilter = point; return *this; }
		/// Sets Tint.
		UINineSlice& SetTint(const glm::vec4& tint) { Tint = tint; return *this; }
	};

	/// The look of the widgets: fonts, colours per state, padding, borders and 9-slice faces. The
	/// default is the engine's theme; set another for the whole UI with DingoUIParams::Style or
	/// DingoUI::SetTheme, for a stretch of the frame with DingoUI::PushStyle, or for one widget
	/// through its params. All sizes are design pixels. Chain the setters.
	struct UIStyle
	{
		/// The font of widget text; null uses the DefaultFont.
		const Font* TextFont = nullptr;
		float TextSize = 18.0f;                                 ///< Height of a line of widget text.
		glm::vec4 TextColor{ 0.95f, 0.96f, 1.0f, 1.0f };        ///< Widget text.
		glm::vec4 DisabledTextColor{ 0.55f, 0.57f, 0.63f, 1.0f }; ///< Text, and handles and fills, of a disabled widget.

		glm::vec4 Background{ 0.22f, 0.27f, 0.42f, 1.0f };      ///< A button's fill at rest.
		glm::vec4 HoverColor{ 0.31f, 0.38f, 0.58f, 1.0f };      ///< Fill under the pointer.
		glm::vec4 PressedColor{ 0.16f, 0.2f, 0.33f, 1.0f };     ///< Fill while held.
		glm::vec4 DisabledColor{ 0.17f, 0.18f, 0.23f, 1.0f };   ///< Fill of a disabled widget.
		/// Reserved: the border colour of the widget that has keyboard or gamepad focus, drawn once focus navigation lands.
		glm::vec4 FocusedColor{ 0.3f, 0.55f, 0.95f, 1.0f };

		glm::vec4 AccentColor{ 0.3f, 0.55f, 0.95f, 1.0f };      ///< A switched-on Toggle, a slider's filled part, a selected row.
		glm::vec4 AccentHoverColor{ 0.4f, 0.64f, 1.0f, 1.0f };  ///< AccentColor under the pointer.
		glm::vec4 TrackColor{ 0.14f, 0.16f, 0.24f, 1.0f };      ///< A switched-off Toggle, a slider's empty part, a ProgressBar's trough.
		glm::vec4 HandleColor{ 0.96f, 0.97f, 1.0f, 1.0f };      ///< The knob of a Toggle and the handle of a Slider.
		glm::vec4 ProgressFillColor{ 0.3f, 0.55f, 0.95f, 1.0f }; ///< The filled part of a ProgressBar.
		glm::vec4 ScrollThumbColor{ 0.8f, 0.85f, 1.0f, 0.45f }; ///< The scroll bar of a scroll area.
		glm::vec4 PanelColor{ 0.09f, 0.11f, 0.17f, 0.95f };     ///< The fill of BeginPanel.

		glm::vec4 BorderColor{ 0.0f };                          ///< Border of buttons, rows and panels.
		float BorderWidth = 0.0f;                               ///< Border thickness; 0 for none.
		float CornerRadius = 6.0f;                              ///< Rounds buttons, rows and panels.

		glm::vec2 Padding{ 14.0f, 7.0f };                       ///< Inside a Button: horizontal, vertical.
		glm::vec2 RowPadding{ 10.0f, 5.0f };                    ///< Inside a Selectable, and after a Toggle: horizontal, vertical.
		float PanelPadding = 12.0f;                             ///< Inside BeginPanel when the box asks for none.
		float Gap = 8.0f;                                       ///< Between a Toggle or Slider and its label.
		glm::vec2 ToggleSize{ 40.0f, 22.0f };                   ///< Width and height of a Toggle's track.
		float HandleSize = 16.0f;                               ///< Diameter of a Toggle's knob and a Slider's handle.
		float SliderHeight = 24.0f;                             ///< Height of a Slider's hit area.
		float SliderTrackHeight = 6.0f;                         ///< Thickness of a Slider's bar.
		glm::vec2 ProgressSize{ 200.0f, 14.0f };               ///< A ProgressBar's size when its params give none.
		float ScrollThumbWidth = 4.0f;                          ///< Width of a scroll area's bar.

		/// Face of a Button and, for a selected row, a Selectable. With an image it replaces Background.
		UINineSlice Surface;
		/// Face under the pointer; with no image, Surface is drawn lighter.
		UINineSlice SurfaceHover;
		/// Face while held; with no image, Surface is drawn darker.
		UINineSlice SurfacePressed;
		/// Face when disabled; with no image, Surface is drawn dimmed.
		UINineSlice SurfaceDisabled;
		/// Face of BeginPanel. With an image it replaces PanelColor.
		UINineSlice Panel;

		/// The engine's theme: what a default-constructed style holds.
		static UIStyle Default() { return UIStyle{}; }

		/// Sets TextFont.
		UIStyle& SetTextFont(const Font* font) { TextFont = font; return *this; }
		/// Sets TextSize.
		UIStyle& SetTextSize(float size) { TextSize = size; return *this; }
		/// Sets TextColor.
		UIStyle& SetTextColor(const glm::vec4& color) { TextColor = color; return *this; }
		/// Sets DisabledTextColor.
		UIStyle& SetDisabledTextColor(const glm::vec4& color) { DisabledTextColor = color; return *this; }
		/// Sets Background.
		UIStyle& SetBackground(const glm::vec4& color) { Background = color; return *this; }
		/// Sets HoverColor.
		UIStyle& SetHoverColor(const glm::vec4& color) { HoverColor = color; return *this; }
		/// Sets PressedColor.
		UIStyle& SetPressedColor(const glm::vec4& color) { PressedColor = color; return *this; }
		/// Sets DisabledColor.
		UIStyle& SetDisabledColor(const glm::vec4& color) { DisabledColor = color; return *this; }
		/// Sets FocusedColor.
		UIStyle& SetFocusedColor(const glm::vec4& color) { FocusedColor = color; return *this; }
		/// Sets AccentColor and AccentHoverColor.
		UIStyle& SetAccent(const glm::vec4& color, const glm::vec4& hover) { AccentColor = color; AccentHoverColor = hover; return *this; }
		/// Sets TrackColor.
		UIStyle& SetTrackColor(const glm::vec4& color) { TrackColor = color; return *this; }
		/// Sets HandleColor.
		UIStyle& SetHandleColor(const glm::vec4& color) { HandleColor = color; return *this; }
		/// Sets ProgressFillColor.
		UIStyle& SetProgressFillColor(const glm::vec4& color) { ProgressFillColor = color; return *this; }
		/// Sets ScrollThumbColor.
		UIStyle& SetScrollThumbColor(const glm::vec4& color) { ScrollThumbColor = color; return *this; }
		/// Sets PanelColor.
		UIStyle& SetPanelColor(const glm::vec4& color) { PanelColor = color; return *this; }
		/// Sets BorderWidth and BorderColor.
		UIStyle& SetBorder(float width, const glm::vec4& color) { BorderWidth = width; BorderColor = color; return *this; }
		/// Sets CornerRadius.
		UIStyle& SetCornerRadius(float radius) { CornerRadius = radius; return *this; }
		/// Sets Padding.
		UIStyle& SetPadding(float horizontal, float vertical) { Padding = { horizontal, vertical }; return *this; }
		/// Sets RowPadding.
		UIStyle& SetRowPadding(float horizontal, float vertical) { RowPadding = { horizontal, vertical }; return *this; }
		/// Sets PanelPadding.
		UIStyle& SetPanelPadding(float padding) { PanelPadding = padding; return *this; }
		/// Sets Gap.
		UIStyle& SetGap(float gap) { Gap = gap; return *this; }
		/// Sets ToggleSize.
		UIStyle& SetToggleSize(float width, float height) { ToggleSize = { width, height }; return *this; }
		/// Sets HandleSize.
		UIStyle& SetHandleSize(float size) { HandleSize = size; return *this; }
		/// Sets SliderHeight.
		UIStyle& SetSliderHeight(float height) { SliderHeight = height; return *this; }
		/// Sets SliderTrackHeight.
		UIStyle& SetSliderTrackHeight(float height) { SliderTrackHeight = height; return *this; }
		/// Sets ProgressSize.
		UIStyle& SetProgressSize(float width, float height) { ProgressSize = { width, height }; return *this; }
		/// Sets ScrollThumbWidth.
		UIStyle& SetScrollThumbWidth(float width) { ScrollThumbWidth = width; return *this; }
		/// Sets Surface.
		UIStyle& SetSurface(const UINineSlice& slice) { Surface = slice; return *this; }
		/// Sets SurfaceHover.
		UIStyle& SetSurfaceHover(const UINineSlice& slice) { SurfaceHover = slice; return *this; }
		/// Sets SurfacePressed.
		UIStyle& SetSurfacePressed(const UINineSlice& slice) { SurfacePressed = slice; return *this; }
		/// Sets SurfaceDisabled.
		UIStyle& SetSurfaceDisabled(const UINineSlice& slice) { SurfaceDisabled = slice; return *this; }
		/// Sets Panel.
		UIStyle& SetPanel(const UINineSlice& slice) { Panel = slice; return *this; }
	};

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
		/// The theme the widgets start from; DingoUI::SetTheme changes it at run time.
		UIStyle Style;
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
		/// A 9-slice drawn over Background and under the children; with no image, nothing extra is drawn.
		UINineSlice NineSlice;

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
		/// Sets NineSlice.
		UIBoxParams& SetNineSlice(const UINineSlice& slice) { NineSlice = slice; return *this; }
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
		/// While this root is declared, the roots below it (lower Layer, or the same Layer and
		/// declared earlier) get no hover or clicks, and every press and scroll, wherever the
		/// pointer is, is taken from the game.
		bool Modal = false;
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
		/// Sets Modal.
		UIRootParams& SetModal(bool modal) { Modal = modal; return *this; }
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

	/// What the pointer did to a widget this frame. Resolved against the previous frame's layout,
	/// so a widget that appears or moves reacts one frame late; one that did not exist last frame
	/// ignores a press that lands on it.
	struct UIInteraction
	{
		bool Hovered = false; ///< The pointer is over it (and no other widget is being held).
		bool Pressed = false; ///< The left button went down on it this frame.
		bool Down = false;    ///< It took a press and the button is still held, wherever the pointer went.
		bool Clicked = false; ///< The button was released over it after going down on it.
		bool Changed = false; ///< A Toggle, Slider or SliderInt changed its value this frame.
		bool Focused = false; ///< Reserved for keyboard and gamepad focus; always false for now.
	};

	/// The size and state of a Button, Toggle, Selectable or ProgressBar. Chain the setters.
	struct UIWidgetParams
	{
		UISize Width;         ///< Width; Fit is the label's width plus padding (a ProgressBar defaults to 200 design pixels).
		UISize Height;        ///< Height; Fit is the label's height plus padding (a ProgressBar defaults to 14 design pixels).
		bool Enabled = true;  ///< Off, the widget is drawn dimmed and ignores the pointer.
		/// A style for this widget alone, instead of the current one; only needs to live for the call.
		const UIStyle* Style = nullptr;

		/// Sets Style.
		UIWidgetParams& SetStyle(const UIStyle* style) { Style = style; return *this; }
		/// Sets Width and Height.
		UIWidgetParams& SetSize(const UISize& width, const UISize& height) { Width = width; Height = height; return *this; }
		/// Sets Width.
		UIWidgetParams& SetWidth(const UISize& width) { Width = width; return *this; }
		/// Sets Height.
		UIWidgetParams& SetHeight(const UISize& height) { Height = height; return *this; }
		/// Sets Enabled.
		UIWidgetParams& SetEnabled(bool enabled) { Enabled = enabled; return *this; }
	};

	/// A Slider or SliderInt. Chain the setters.
	struct UISliderParams
	{
		UISize Width = UISize::Fixed(200.0f); ///< Width of the track and handle.
		float Step = 0.0f;                    ///< Values snap to multiples of this from the minimum; 0 for none (SliderInt always steps by 1).
		bool ShowValue = true;                ///< Draw the value after the track.
		bool Enabled = true;                  ///< Off, the slider is drawn dimmed and ignores the pointer.
		/// A style for this slider alone, instead of the current one; only needs to live for the call.
		const UIStyle* Style = nullptr;

		/// Sets Style.
		UISliderParams& SetStyle(const UIStyle* style) { Style = style; return *this; }
		/// Sets Width.
		UISliderParams& SetWidth(const UISize& width) { Width = width; return *this; }
		/// Sets Step.
		UISliderParams& SetStep(float step) { Step = step; return *this; }
		/// Sets ShowValue.
		UISliderParams& SetShowValue(bool show) { ShowValue = show; return *this; }
		/// Sets Enabled.
		UISliderParams& SetEnabled(bool enabled) { Enabled = enabled; return *this; }
	};

	/// A vertically scrolling area. Chain the setters.
	struct UIScrollParams
	{
		/// The area's own layout; give it a Height (Fixed, or Percent/Grow in a sized parent) or it
		/// grows to fit its content and never scrolls. ClipChildren is always on.
		UIBoxParams Layout;
		float WheelStep = 40.0f;  ///< Design pixels one wheel notch scrolls.
		bool DragScroll = false;  ///< Dragging the area's empty background scrolls it.
		/// A style for this area's bar alone, instead of the current one; only needs to live for the call.
		const UIStyle* Style = nullptr;

		/// Sets Layout.
		UIScrollParams& SetLayout(const UIBoxParams& layout) { Layout = layout; return *this; }
		/// Sets WheelStep.
		UIScrollParams& SetWheelStep(float step) { WheelStep = step; return *this; }
		/// Sets DragScroll.
		UIScrollParams& SetDragScroll(bool drag) { DragScroll = drag; return *this; }
		/// Sets Style.
		UIScrollParams& SetStyle(const UIStyle* style) { Style = style; return *this; }
	};

	/// The game UI: HUDs and menus declared every frame from OnUpdate, laid out, drawn over the
	/// scene and its post chain, and taking the pointer from the game where it lands on them.
	/// Static, like Input; call it from the main thread. ImGui stays the debug UI (Dingo::UI).
	///
	/// A frame declares roots, each holding boxes, text, images and widgets; the engine lays them out after
	/// the last OnUpdate and draws them in one pass. Every Begin must be matched by its End, and
	/// the End is called only when the Begin returned true:
	/// @code
	/// if (DingoUI::BeginRoot("hud", UIRootParams().SetAnchor(UIAnchor::TopLeft).SetOffset({ 16, 16 })))
	/// {
	///     DingoUI::Text("Score 120");
	///     if (DingoUI::Button("Pause").Clicked) m_Paused = true;
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

		/// Adds a push button showing `label` to the current container. The label is also the
		/// widget's id: text after `##` is part of the id but not shown, so two buttons that read
		/// the same need `"OK##first"` and `"OK##second"` (or a PushId around each).
		static UIInteraction Button(std::string_view label, const UIWidgetParams& params = {});
		/// Adds a switch with `label`; a click flips `*value` and sets Changed. A null `value` draws it off.
		static UIInteraction Toggle(std::string_view label, bool* value, const UIWidgetParams& params = {});
		/// Adds a horizontal slider with `label` that edits `*value` between `min` and `max`, by
		/// click or drag. The value is clamped (and snapped to params.Step) on every
		/// call; Changed is set when it moved. A null `value` draws it at `min`.
		static UIInteraction Slider(std::string_view label, float* value, float min, float max, const UISliderParams& params = {});
		/// As Slider, for whole numbers.
		static UIInteraction SliderInt(std::string_view label, int* value, int min, int max, const UISliderParams& params = {});
		/// Adds a bar filled `value` (0 to 1, clamped) of the way; `id` names its element and shows
		/// nowhere. Takes no input, but a bar in a blocking root still blocks the pointer.
		static void ProgressBar(std::string_view id, float value, const UIWidgetParams& params = {});
		/// Adds a list row showing `label`, highlighted while `selected`. The game stores the
		/// selection: set it when Clicked is true.
		static UIInteraction Selectable(std::string_view label, bool selected, const UIWidgetParams& params = {});
		/// Opens a vertically scrolling, clipped container named `label` inside the current
		/// container. The mouse wheel over it scrolls it and is taken from the game. Returns false,
		/// and EndScrollArea must not be called, when no root is open.
		static bool BeginScrollArea(std::string_view label, const UIScrollParams& params = {});
		/// Closes the area opened by the matching BeginScrollArea.
		static void EndScrollArea();
		/// Opens a container dressed as a panel: the current style's PanelColor (or Panel 9-slice),
		/// border, corner radius and PanelPadding, for whatever `params` leaves at its default.
		/// Returns false, and EndPanel must not be called, when no root is open.
		static bool BeginPanel(const UIBoxParams& params = {});
		/// Closes the panel opened by the matching BeginPanel.
		static void EndPanel();

		/// Makes `style` the current style until the matching PopStyle: widgets and panels declared
		/// meanwhile use it. Styles nest; unpopped ones are dropped, with one warning, at the end of the frame.
		static void PushStyle(const UIStyle& style);
		/// Returns to the style before the last PushStyle. Pops beyond the theme are ignored.
		static void PopStyle();
		/// The current style: the last pushed, or the theme.
		static const UIStyle& GetStyle();
		/// Replaces the theme, the style when nothing is pushed.
		static void SetTheme(const UIStyle& style);

		/// Mixes `id` into the ids of the widgets declared until the matching PopId, so a loop can
		/// give each iteration's widgets their own id. Pops that outnumber pushes inside a
		/// container are ignored.
		static void PushId(std::string_view id);
		/// As PushId, for a number such as a loop index.
		static void PushId(int id);
		/// Removes the id pushed last. Ids pushed inside a box or root are dropped when it ends.
		static void PopId();

		/// Sets the font Text uses when its params name none. Pass nullptr before deleting the font.
		static void SetDefaultFont(Font* font);
		/// The font set by DingoUIParams::DefaultFont or SetDefaultFont, or nullptr.
		static Font* GetDefaultFont();

		/// True while the pointer is over an element that blocks input (as of the last frame's
		/// layout) or a modal root is up. A press or scroll that lands there is hidden from Input
		/// (the button reads up until it is released); this is for a game that wants the picture anyway.
		static bool IsPointerOverUI();
		/// True while the UI holds the pointer (a press it took and has not released, a widget being
		/// held, or a modal root), so the game should leave that input alone.
		static bool IsCapturingInput();
	};

}
