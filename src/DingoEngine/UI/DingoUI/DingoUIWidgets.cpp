#include "depch.h"
#include "DingoUIContext.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace Dingo::Internal
{

	namespace
	{

		constexpr float k_ValueWidth = 56.0f;
		constexpr float k_CaretWidth = 2.0f;
		constexpr float k_CaretBlinkPeriod = 1.0f;

		bool IsContinuation(char byte)
		{
			return (static_cast<uint8_t>(byte) & 0xC0) == 0x80;
		}

		size_t PrevBoundary(std::string_view text, size_t at)
		{
			if (at == 0)
				return 0;
			--at;
			while (at > 0 && IsContinuation(text[at]))
				--at;
			return at;
		}

		size_t NextBoundary(std::string_view text, size_t at)
		{
			if (at >= text.size())
				return text.size();
			++at;
			while (at < text.size() && IsContinuation(text[at]))
				++at;
			return at;
		}

		size_t CountCodepoints(std::string_view text)
		{
			size_t count = 0;
			for (const char byte : text)
				count += IsContinuation(byte) ? 0 : 1;
			return count;
		}

		bool IsTypeable(char32_t codepoint)
		{
			return codepoint >= 0x20 && !(codepoint >= 0x7F && codepoint <= 0x9F) && codepoint <= 0x10FFFF && !(codepoint >= 0xD800 && codepoint <= 0xDFFF);
		}

		size_t EncodeUtf8(char32_t codepoint, char* out)
		{
			if (codepoint < 0x80)
			{
				out[0] = static_cast<char>(codepoint);
				return 1;
			}
			if (codepoint < 0x800)
			{
				out[0] = static_cast<char>(0xC0 | (codepoint >> 6));
				out[1] = static_cast<char>(0x80 | (codepoint & 0x3F));
				return 2;
			}
			if (codepoint < 0x10000)
			{
				out[0] = static_cast<char>(0xE0 | (codepoint >> 12));
				out[1] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
				out[2] = static_cast<char>(0x80 | (codepoint & 0x3F));
				return 3;
			}
			out[0] = static_cast<char>(0xF0 | (codepoint >> 18));
			out[1] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
			out[2] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
			out[3] = static_cast<char>(0x80 | (codepoint & 0x3F));
			return 4;
		}

		std::string_view VisibleLabel(std::string_view label)
		{
			const size_t suffix = label.find("##");
			return suffix == std::string_view::npos ? label : label.substr(0, suffix);
		}

		enum class Face
		{
			Idle,
			Hover,
			Pressed,
			Disabled
		};

		Face FaceOf(const UIInteraction& state, bool enabled)
		{
			if (!enabled)
				return Face::Disabled;
			if (state.Down && state.Hovered)
				return Face::Pressed;
			return state.Hovered ? Face::Hover : Face::Idle;
		}

		glm::vec4 Surface(const UIStyle& style, Face face)
		{
			switch (face)
			{
			case Face::Disabled: return style.DisabledColor;
			case Face::Pressed: return style.PressedColor;
			case Face::Hover: return style.HoverColor;
			case Face::Idle: break;
			}
			return style.Background;
		}

		UINineSlice SliceFor(const UIStyle& style, Face face)
		{
			const UINineSlice* own = nullptr;
			glm::vec3 shade(1.0f);
			switch (face)
			{
			case Face::Disabled: own = &style.SurfaceDisabled; shade = glm::vec3(0.5f); break;
			case Face::Pressed: own = &style.SurfacePressed; shade = glm::vec3(0.8f); break;
			case Face::Hover: own = &style.SurfaceHover; shade = glm::vec3(1.15f); break;
			case Face::Idle: break;
			}
			if (own && own->IsValid())
				return *own;
			UINineSlice slice = style.Surface;
			slice.Tint = glm::vec4(glm::vec3(slice.Tint) * shade, slice.Tint.a);
			return slice;
		}

		// A slice replaces the fill; the border and corner radius stay as the style has them.
		void ApplyFace(UIBoxParams& box, const UIStyle& style, Face face)
		{
			if (style.Surface.IsValid())
				box.NineSlice = SliceFor(style, face);
			else
				box.Background = Surface(style, face);
			box.BorderWidth = style.BorderWidth;
			box.BorderColor = style.BorderColor;
			box.CornerRadius = style.CornerRadius;
		}

		UITextParams LabelStyle(const UIStyle& style, bool enabled)
		{
			return UITextParams().SetSize(style.TextSize).SetColor(enabled ? style.TextColor : style.DisabledTextColor).SetWrap(false).SetFont(style.TextFont);
		}

		// A mix of the idle face and the hover and pressed ones; the 9-slice faces of their own can't be mixed, so they switch halfway.
		void ApplyFaceBlend(UIBoxParams& box, const UIStyle& style, float hover, float press)
		{
			if (style.Surface.IsValid())
			{
				if (style.SurfaceHover.IsValid() || style.SurfacePressed.IsValid())
				{
					box.NineSlice = SliceFor(style, press >= 0.5f ? Face::Pressed : (hover >= 0.5f ? Face::Hover : Face::Idle));
				}
				else
				{
					box.NineSlice = style.Surface;
					const float shade = glm::mix(glm::mix(1.0f, 1.15f, hover), 0.8f, press);
					box.NineSlice.Tint = glm::vec4(glm::vec3(box.NineSlice.Tint) * shade, box.NineSlice.Tint.a);
				}
			}
			else
				box.Background = glm::mix(glm::mix(style.Background, style.HoverColor, hover), style.PressedColor, press);
			box.BorderWidth = style.BorderWidth;
			box.BorderColor = style.BorderColor;
			box.CornerRadius = style.CornerRadius;
		}

		// How much of the focus ring shows, 0..1.
		void ApplyRing(UIBoxParams& box, const UIStyle& style, float amount)
		{
			if (amount <= 0.0f || style.FocusWidth <= 0.0f)
				return;
			box.BorderColor = style.FocusedColor;
			box.BorderColor.a *= amount;
			box.BorderWidth = (std::max)(box.BorderWidth, style.FocusWidth);
			box.CornerRadius = (std::max)(box.CornerRadius, style.CornerRadius);
		}

		UIBoxParams Solid(const UISize& width, const UISize& height, const glm::vec4& color, float radius)
		{
			UIBoxParams box;
			box.Width = width;
			box.Height = height;
			box.Background = color;
			box.CornerRadius = radius;
			return box;
		}

	}

	UIInteraction DingoUIContext::Button(std::string_view label, const UIWidgetParams& params)
	{
		const uint64_t id = WidgetId(label);
		if (id == 0)
			return {};

		UIWidgetState* state = nullptr;
		const UIInteraction result = Interact(id, label, params.Enabled, state);

		const UIStyle& style = Resolve(params.Style);
		UIBoxParams box;
		box.Width = params.Width;
		box.Height = params.Height;
		box.Padding = { style.Padding.x, style.Padding.y, style.Padding.x, style.Padding.y };
		box.Justify = UIJustify::Center;
		box.Align = UIAlign::Center;
		const std::array<float, 4> look = StepLook(*state, style, result.Hovered, result.Down && result.Hovered, result.Focused, false);
		const bool animate = style.TransitionDuration > 0.0f && params.Enabled;
		if (animate)
			ApplyFaceBlend(box, style, look[0], look[1]);
		else
			ApplyFace(box, style, FaceOf(result, params.Enabled));
		ApplyRing(box, style, animate ? look[2] : (result.Focused ? 1.0f : 0.0f));
		m_WidgetFocus = { params.Enabled && params.Focusable, params.DefaultFocus, false };
		BeginWidgetBox(id, state, label, box, params.Enabled);
		Text(VisibleLabel(label), LabelStyle(style, params.Enabled));
		m_Tree.EndBox();
		return result;
	}

	UIInteraction DingoUIContext::Toggle(std::string_view label, bool* value, const UIWidgetParams& params)
	{
		const uint64_t id = WidgetId(label);
		if (id == 0)
			return {};

		bool fallback = false;
		bool* target = value ? value : &fallback;

		UIWidgetState* state = nullptr;
		UIInteraction result = Interact(id, label, params.Enabled, state);
		if (result.Clicked)
		{
			*target = !*target;
			result.Changed = true;
		}
		const bool on = *target;

		const UIStyle& style = Resolve(params.Style);
		UIBoxParams row;
		row.Width = params.Width;
		row.Height = params.Height;
		row.Direction = UIDirection::Row;
		row.Gap = style.Gap;
		row.Align = UIAlign::Center;
		row.Padding = { 0.0f, style.RowPadding.y * 0.5f, style.RowPadding.x, style.RowPadding.y * 0.5f };
		const std::array<float, 4> look = StepLook(*state, style, result.Hovered, false, result.Focused, on);
		const bool animate = style.TransitionDuration > 0.0f && params.Enabled;
		ApplyRing(row, style, animate ? look[2] : (result.Focused ? 1.0f : 0.0f));
		m_WidgetFocus = { params.Enabled && params.Focusable, params.DefaultFocus, false };
		BeginWidgetBox(id, state, label, row, params.Enabled);

		glm::vec4 trackColor = style.TrackColor;
		if (!params.Enabled)
			trackColor = style.DisabledColor;
		else if (animate)
			trackColor = glm::mix(glm::mix(style.TrackColor, style.HoverColor, look[0]), glm::mix(style.AccentColor, style.AccentHoverColor, look[0]), look[3]);
		else if (on)
			trackColor = result.Hovered ? style.AccentHoverColor : style.AccentColor;
		else if (result.Hovered)
			trackColor = style.HoverColor;

		UIBoxParams track = Solid(UISize::Fixed(style.ToggleSize.x), UISize::Fixed(style.ToggleSize.y), trackColor, style.ToggleSize.y * 0.5f);
		track.Direction = UIDirection::Row;
		track.Align = UIAlign::Center;
		track.Justify = on ? UIJustify::End : UIJustify::Start;
		track.Padding = glm::vec4((style.ToggleSize.y - style.HandleSize) * 0.5f);
		m_Tree.BeginBox(track);
		m_Tree.BeginBox(Solid(UISize::Fixed(style.HandleSize), UISize::Fixed(style.HandleSize), params.Enabled ? style.HandleColor : style.DisabledTextColor, style.HandleSize * 0.5f));
		m_Tree.EndBox();
		m_Tree.EndBox();

		const std::string_view text = VisibleLabel(label);
		if (!text.empty())
			Text(text, LabelStyle(style, params.Enabled));
		m_Tree.EndBox();
		return result;
	}

	UIInteraction DingoUIContext::SliderImpl(std::string_view label, double* value, double min, double max, double step, bool integer, const UISliderParams& params)
	{
		const uint64_t id = WidgetId(label);
		if (id == 0)
			return {};

		UIWidgetState* state = nullptr;
		UIInteraction result = Interact(id, label, params.Enabled, state);
		const UIStyle& style = Resolve(params.Style);

		const double low = min;
		const double high = (std::max)(max, min);
		double current = std::clamp(*value, low, high);
		if (params.Enabled && high > low && (result.Pressed || result.Down || id == m_ReleasedId))
		{
			const float handle = style.HandleSize * m_Tree.GetRootScale();
			const float travel = (state->LastRect.Max.x - state->LastRect.Min.x) - handle;
			if (travel > 0.5f)
			{
				const double t = std::clamp(static_cast<double>((m_Input.PointerPosition.x - state->LastRect.Min.x - handle * 0.5f) / travel), 0.0, 1.0);
				double picked = low + t * (high - low);
				if (step > 0.0)
					picked = low + std::round((picked - low) / step) * step;
				const double next = std::clamp(picked, low, high);
				result.Changed = next != *value;
				current = next;
			}
		}
		if (params.Enabled && high > low && m_NavAdjust != 0 && id == m_FocusId)
		{
			const double increment = integer ? 1.0 : (step > 0.0 ? step : (high - low) * 0.05);
			double next = current + m_NavAdjust * increment;
			if (step > 0.0)
				next = low + std::round((next - low) / step) * step;
			next = std::clamp(next, low, high);
			if (integer)
				next = std::round(next);
			if (next != current)
			{
				result.Changed = true;
				current = next;
			}
		}
		if (integer)
			current = std::round(current);
		*value = current;

		const float t = high > low ? static_cast<float>((current - low) / (high - low)) : 0.0f;

		UIBoxParams outer;
		outer.Direction = UIDirection::Row;
		outer.Gap = style.Gap;
		outer.Align = UIAlign::Center;
		const std::array<float, 4> look = StepLook(*state, style, result.Hovered || result.Down, false, result.Focused, false);
		const bool animate = style.TransitionDuration > 0.0f && params.Enabled;
		ApplyRing(outer, style, animate ? look[2] : (result.Focused ? 1.0f : 0.0f));
		m_Tree.BeginBox(outer);

		UIBoxParams track;
		track.Width = params.Width;
		track.Height = UISize::Fixed(style.SliderHeight);
		track.Direction = UIDirection::Row;
		track.Align = UIAlign::Center;
		m_WidgetFocus = { params.Enabled && params.Focusable, params.DefaultFocus, true };
		BeginWidgetBox(id, state, label, track, params.Enabled);

		const auto leaf = [this](const UIBoxParams& box)
		{
			m_Tree.BeginBox(box);
			m_Tree.EndBox();
		};
		const UISize barHeight = UISize::Fixed(style.SliderTrackHeight);
		const float barRadius = style.SliderTrackHeight * 0.5f;
		const glm::vec4 fillColor = !params.Enabled ? style.DisabledTextColor : (animate ? glm::mix(style.AccentColor, style.AccentHoverColor, look[0]) : ((result.Hovered || result.Down) ? style.AccentHoverColor : style.AccentColor));
		if (t > 0.0f)
			leaf(Solid(UISize::Grow(t), barHeight, fillColor, barRadius));
		leaf(Solid(UISize::Fixed(style.HandleSize), UISize::Fixed(style.HandleSize), params.Enabled ? style.HandleColor : style.DisabledTextColor, style.HandleSize * 0.5f));
		if (t < 1.0f)
			leaf(Solid(UISize::Grow(1.0f - t), barHeight, params.Enabled ? style.TrackColor : style.DisabledColor, barRadius));
		m_Tree.EndBox();

		if (params.ShowValue)
		{
			const std::string text = integer ? std::format("{}", static_cast<long long>(current)) : (step >= 1.0 && step == std::floor(step) ? std::format("{:.0f}", current) : std::format("{:.2f}", current));
			Text(text, LabelStyle(style, params.Enabled).SetWidth(UISize::Fixed(k_ValueWidth)).SetAlign(UITextAlign::Right));
		}
		const std::string_view caption = VisibleLabel(label);
		if (!caption.empty())
			Text(caption, LabelStyle(style, params.Enabled));
		m_Tree.EndBox();
		return result;
	}

	UIInteraction DingoUIContext::Slider(std::string_view label, float* value, float min, float max, const UISliderParams& params)
	{
		double current = value ? static_cast<double>(*value) : static_cast<double>(min);
		const UIInteraction result = SliderImpl(label, &current, min, max, params.Step, false, params);
		if (value)
			*value = static_cast<float>(current);
		return result;
	}

	UIInteraction DingoUIContext::SliderInt(std::string_view label, int* value, int min, int max, const UISliderParams& params)
	{
		double current = value ? static_cast<double>(*value) : static_cast<double>(min);
		const UIInteraction result = SliderImpl(label, &current, min, max, 1.0, true, params);
		if (value)
			*value = static_cast<int>(current);
		return result;
	}

	void DingoUIContext::ProgressBar(std::string_view idText, float value, const UIWidgetParams& params)
	{
		const uint64_t id = WidgetId(idText);
		if (id == 0)
			return;

		UIWidgetState* state = nullptr;
		Interact(id, idText, false, state);

		const auto leaf = [this](const UIBoxParams& box)
		{
			m_Tree.BeginBox(box);
			m_Tree.EndBox();
		};
		const UIStyle& style = Resolve(params.Style);
		const float t = std::clamp(value, 0.0f, 1.0f);
		const UISize width = params.Width.Mode == UISizeMode::Fit ? UISize::Fixed(style.ProgressSize.x) : params.Width;
		const UISize height = params.Height.Mode == UISizeMode::Fit ? UISize::Fixed(style.ProgressSize.y) : params.Height;

		UIBoxParams bar = Solid(width, height, params.Enabled ? style.TrackColor : style.DisabledColor, style.ProgressSize.y * 0.5f);
		bar.Direction = UIDirection::Row;
		BeginWidgetBox(id, state, idText, bar, false);
		if (t > 0.0f)
			leaf(Solid(UISize::Grow(t), UISize::Grow(1.0f), params.Enabled ? style.ProgressFillColor : style.DisabledTextColor, style.ProgressSize.y * 0.5f));
		if (t > 0.0f && t < 1.0f)
			leaf(UIBoxParams().SetWidth(UISize::Grow(1.0f - t)));
		m_Tree.EndBox();
	}

	UIInteraction DingoUIContext::Selectable(std::string_view label, bool selected, const UIWidgetParams& params)
	{
		const uint64_t id = WidgetId(label);
		if (id == 0)
			return {};

		UIWidgetState* state = nullptr;
		const UIInteraction result = Interact(id, label, params.Enabled, state);

		const UIStyle& style = Resolve(params.Style);
		UIBoxParams box;
		box.Width = params.Width;
		box.Height = params.Height;
		box.Padding = { style.RowPadding.x, style.RowPadding.y, style.RowPadding.x, style.RowPadding.y };
		box.Justify = UIJustify::Center;
		box.BorderWidth = style.BorderWidth;
		box.BorderColor = style.BorderColor;
		box.CornerRadius = style.CornerRadius;

		const std::array<float, 4> look = StepLook(*state, style, result.Hovered, result.Down && result.Hovered, result.Focused, selected);
		const bool animate = style.TransitionDuration > 0.0f && params.Enabled && !style.Surface.IsValid();
		if (animate)
		{
			const glm::vec4 rest = glm::mix(glm::vec4(glm::vec3(style.HoverColor), 0.0f), glm::mix(style.HoverColor, style.PressedColor, look[1]), look[0]);
			box.Background = glm::mix(rest, glm::mix(style.AccentColor, style.AccentHoverColor, look[0]), look[3]);
		}
		else
		{
			if (style.Surface.IsValid())
			{
				if (!params.Enabled)
				{
					if (selected)
						box.NineSlice = SliceFor(style, Face::Disabled);
				}
				else if (result.Hovered && result.Down)
					box.NineSlice = SliceFor(style, Face::Pressed);
				else if (result.Hovered)
					box.NineSlice = SliceFor(style, Face::Hover);
				else if (selected)
					box.NineSlice = SliceFor(style, Face::Idle);
			}
			else if (!params.Enabled)
				box.Background = selected ? style.DisabledColor : glm::vec4(0.0f);
			else if (selected)
				box.Background = result.Hovered ? style.AccentHoverColor : style.AccentColor;
			else if (result.Hovered)
				box.Background = result.Down ? style.PressedColor : style.HoverColor;
		}
		ApplyRing(box, style, style.TransitionDuration > 0.0f && params.Enabled ? look[2] : (result.Focused ? 1.0f : 0.0f));
		m_WidgetFocus = { params.Enabled && params.Focusable, params.DefaultFocus, false };
		BeginWidgetBox(id, state, label, box, params.Enabled);
		Text(VisibleLabel(label), LabelStyle(style, params.Enabled));
		m_Tree.EndBox();
		return result;
	}

	UIInteraction DingoUIContext::TextField(std::string_view label, std::string* text, const UITextFieldParams& params)
	{
		const uint64_t id = WidgetId(label);
		if (id == 0)
			return {};

		std::string fallback;
		std::string& value = text ? *text : fallback;

		UIWidgetState* state = nullptr;
		UIInteraction result = Interact(id, label, params.Enabled, state);
		const UIStyle& style = Resolve(params.Style);
		const Font* font = style.TextFont ? style.TextFont : m_DefaultFont;
		const float scale = m_Tree.GetRootScale();
		const float textSize = style.TextSize * scale;
		const float padding = style.Padding.x * scale;
		const auto measure = [&](std::string_view part) { return m_Measure(font, part, textSize, 0.0f).x; };

		if (m_EditId == id && !params.Enabled)
			m_EditId = 0;
		bool editing = m_EditId == id;
		const bool readKeys = editing && m_EditStartFrame != m_FrameIndex;
		if (!editing && params.Enabled && (result.Pressed || id == m_NavAcceptId))
		{
			m_EditId = id;
			m_EditStartFrame = m_FrameIndex;
			m_EditOriginal = value;
			m_EditCaret = value.size();
			m_EditBlink = 0.0f;
			editing = true;
		}

		size_t caret = value.size();
		if (editing)
		{
			caret = (std::min)(m_EditCaret, value.size());
			while (caret > 0 && caret < value.size() && IsContinuation(value[caret]))
				--caret;

			bool moved = false;
			if (result.Pressed && state->LastRect.Max.x > state->LastRect.Min.x)
			{
				const float x = m_Input.PointerPosition.x - (state->LastRect.Min.x + padding) + state->ScrollOffset.x;
				size_t best = 0;
				float bestDistance = std::abs(x);
				for (size_t at = NextBoundary(value, 0); at > 0; at = at < value.size() ? NextBoundary(value, at) : 0)
				{
					const float width = measure(std::string_view(value).substr(0, at));
					if (std::abs(x - width) < bestDistance)
					{
						best = at;
						bestDistance = std::abs(x - width);
					}
					if (width > x)
						break;
				}
				caret = best;
				moved = true;
			}

			if (readKeys)
			{
				const auto pulse = [this](UITextKey key) { return m_TextKeyPulse[static_cast<size_t>(key)]; };
				bool changed = false;
				for (const char32_t codepoint : m_Input.Typed)
				{
					if (!IsTypeable(codepoint))
						continue;
					if (params.MaxLength > 0 && CountCodepoints(value) >= params.MaxLength)
						break;
					char bytes[4];
					const size_t length = EncodeUtf8(codepoint, bytes);
					value.insert(caret, bytes, length);
					caret += length;
					changed = true;
				}
				if (pulse(UITextKey::Backspace) && caret > 0)
				{
					const size_t from = PrevBoundary(value, caret);
					value.erase(from, caret - from);
					caret = from;
					changed = true;
				}
				if (pulse(UITextKey::Delete) && caret < value.size())
				{
					value.erase(caret, NextBoundary(value, caret) - caret);
					changed = true;
				}
				if (pulse(UITextKey::Left))
				{
					caret = PrevBoundary(value, caret);
					moved = true;
				}
				if (pulse(UITextKey::Right))
				{
					caret = NextBoundary(value, caret);
					moved = true;
				}
				if (pulse(UITextKey::Home))
				{
					caret = 0;
					moved = true;
				}
				if (pulse(UITextKey::End))
				{
					caret = value.size();
					moved = true;
				}

				if (pulse(UITextKey::Escape) || m_EditPadBack)
				{
					if (value != m_EditOriginal)
					{
						value = m_EditOriginal;
						changed = true;
					}
					result.Cancelled = true;
				}
				else if (pulse(UITextKey::Enter) || m_EditPadAccept)
					result.Submitted = true;
				if (result.Cancelled || result.Submitted)
				{
					m_EditId = 0;
					editing = false;
					caret = value.size();
				}
				result.Changed = changed;
				if (changed || moved)
					m_EditBlink = 0.0f;
			}
			else if (moved)
				m_EditBlink = 0.0f;
			if (editing)
				m_EditCaret = caret;
		}

		float scroll = 0.0f;
		const float inner = (state->LastRect.Max.x - state->LastRect.Min.x) - 2.0f * padding - k_CaretWidth * scale;
		if (editing && inner > 0.0f)
		{
			const float caretX = measure(std::string_view(value).substr(0, caret));
			scroll = state->ScrollOffset.x;
			if (caretX - scroll > inner)
				scroll = caretX - inner;
			if (caretX < scroll)
				scroll = caretX;
			scroll = std::clamp(scroll, 0.0f, (std::max)(0.0f, measure(value) - inner));
		}
		state->ScrollOffset.x = scroll;

		UIBoxParams box;
		box.Width = params.Width;
		box.Direction = UIDirection::Row;
		box.Align = UIAlign::Center;
		box.Padding = { style.Padding.x, style.Padding.y, style.Padding.x, style.Padding.y };
		box.ClipChildren = true;
		ApplyFace(box, style, !params.Enabled ? Face::Disabled : (result.Hovered ? Face::Hover : Face::Idle));
		ApplyRing(box, style, editing || result.Focused ? 1.0f : 0.0f);
		m_WidgetFocus = { params.Enabled && params.Focusable, params.DefaultFocus, false };
		BeginWidgetBox(id, state, label, box, params.Enabled, false, glm::vec2(scroll, 0.0f));

		const UITextParams look = LabelStyle(style, params.Enabled);
		const std::string_view shown = value;
		if (caret > 0)
			Text(shown.substr(0, caret), look);
		const bool caretOn = editing && std::fmod(m_EditBlink, k_CaretBlinkPeriod) < k_CaretBlinkPeriod * 0.5f;
		m_Tree.BeginBox(Solid(UISize::Fixed(k_CaretWidth), UISize::Fixed(style.TextSize), caretOn ? style.TextColor : glm::vec4(0.0f), 0.0f));
		m_Tree.EndBox();
		if (caret < shown.size())
			Text(shown.substr(caret), look);
		else if (shown.empty() && !params.Placeholder.empty())
			Text(params.Placeholder, LabelStyle(style, false));
		m_Tree.EndBox();
		return result;
	}

	bool DingoUIContext::BeginScrollArea(std::string_view label, const UIScrollParams& params)
	{
		const uint64_t id = WidgetId(label);
		if (id == 0)
			return false;

		UIWidgetState* state = nullptr;
		const UIInteraction result = Interact(id, label, params.DragScroll, state);

		const float scale = m_Tree.GetRootScale();
		glm::vec2 offset = state->ScrollOffset;
		if (m_ScrollTargetId == id && m_ScrollDelta.y != 0.0f)
			offset.y -= m_ScrollDelta.y * params.WheelStep * scale;
		if (params.DragScroll)
		{
			if (result.Pressed)
				state->PointerLast = m_Input.PointerPosition;
			else if (result.Down)
			{
				offset.y -= m_Input.PointerPosition.y - state->PointerLast.y;
				state->PointerLast = m_Input.PointerPosition;
			}
		}

		const float range = (std::max)(0.0f, state->ContentSize.y - (state->LastRect.Max.y - state->LastRect.Min.y));
		offset.x = 0.0f;
		offset.y = std::clamp(offset.y, 0.0f, range);
		state->ScrollOffset = offset;

		BeginWidgetBox(id, state, label, params.Layout, params.DragScroll, true, offset, params.Style);
		m_ScrollStack.push_back(id);
		return true;
	}

	void DingoUIContext::EndScrollArea()
	{
		if (!m_InFrame)
			return;
		if (m_ScrollStack.empty() || !m_Tree.EndBox())
		{
			WarnMismatch("EndScrollArea without a matching BeginScrollArea, or with a box still open; the call was ignored");
			return;
		}
		m_ScrollStack.pop_back();
	}

}
