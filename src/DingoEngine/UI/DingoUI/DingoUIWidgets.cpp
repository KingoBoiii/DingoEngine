#include "depch.h"
#include "DingoUIContext.h"
#include "DingoUIWidgetLook.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace Dingo::Internal
{

	namespace
	{

		std::string_view VisibleLabel(std::string_view label)
		{
			const size_t suffix = label.find("##");
			return suffix == std::string_view::npos ? label : label.substr(0, suffix);
		}

		glm::vec4 Surface(const UIInteraction& state, bool enabled)
		{
			if (!enabled)
				return UILook::k_Disabled;
			if (state.Down && state.Hovered)
				return UILook::k_Pressed;
			return state.Hovered ? UILook::k_Hover : UILook::k_Idle;
		}

		UITextParams LabelStyle(bool enabled)
		{
			return UITextParams().SetSize(UILook::k_TextSize).SetColor(enabled ? UILook::k_Text : UILook::k_TextDisabled).SetWrap(false);
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

		UIBoxParams box;
		box.Width = params.Width;
		box.Height = params.Height;
		box.Padding = { UILook::k_ButtonPaddingX, UILook::k_ButtonPaddingY, UILook::k_ButtonPaddingX, UILook::k_ButtonPaddingY };
		box.Justify = UIJustify::Center;
		box.Align = UIAlign::Center;
		box.Background = Surface(result, params.Enabled);
		box.CornerRadius = UILook::k_CornerRadius;
		BeginWidgetBox(id, label, box, params.Enabled);
		Text(VisibleLabel(label), LabelStyle(params.Enabled));
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

		UIBoxParams row;
		row.Width = params.Width;
		row.Height = params.Height;
		row.Direction = UIDirection::Row;
		row.Gap = UILook::k_Gap;
		row.Align = UIAlign::Center;
		row.Padding = { 0.0f, UILook::k_RowPaddingY * 0.5f, UILook::k_RowPaddingX, UILook::k_RowPaddingY * 0.5f };
		BeginWidgetBox(id, label, row, params.Enabled);

		glm::vec4 trackColor = UILook::k_Track;
		if (!params.Enabled)
			trackColor = UILook::k_Disabled;
		else if (on)
			trackColor = result.Hovered ? UILook::k_AccentHover : UILook::k_Accent;
		else if (result.Hovered)
			trackColor = UILook::k_Hover;

		UIBoxParams track = Solid(UISize::Fixed(UILook::k_ToggleWidth), UISize::Fixed(UILook::k_ToggleHeight), trackColor, UILook::k_ToggleHeight * 0.5f);
		track.Direction = UIDirection::Row;
		track.Align = UIAlign::Center;
		track.Justify = on ? UIJustify::End : UIJustify::Start;
		track.Padding = glm::vec4((UILook::k_ToggleHeight - UILook::k_ToggleKnob) * 0.5f);
		m_Tree.BeginBox(track);
		m_Tree.BeginBox(Solid(UISize::Fixed(UILook::k_ToggleKnob), UISize::Fixed(UILook::k_ToggleKnob), params.Enabled ? UILook::k_Knob : UILook::k_TextDisabled, UILook::k_ToggleKnob * 0.5f));
		m_Tree.EndBox();
		m_Tree.EndBox();

		const std::string_view text = VisibleLabel(label);
		if (!text.empty())
			Text(text, LabelStyle(params.Enabled));
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

		const double low = min;
		const double high = (std::max)(max, min);
		double current = std::clamp(*value, low, high);
		if (params.Enabled && high > low && (result.Pressed || result.Down || id == m_ReleasedId))
		{
			const float handle = UILook::k_SliderHandle * m_Tree.GetRootScale();
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
		if (integer)
			current = std::round(current);
		*value = current;

		const float t = high > low ? static_cast<float>((current - low) / (high - low)) : 0.0f;

		UIBoxParams outer;
		outer.Direction = UIDirection::Row;
		outer.Gap = UILook::k_Gap;
		outer.Align = UIAlign::Center;
		m_Tree.BeginBox(outer);

		UIBoxParams track;
		track.Width = params.Width;
		track.Height = UISize::Fixed(UILook::k_SliderHeight);
		track.Direction = UIDirection::Row;
		track.Align = UIAlign::Center;
		BeginWidgetBox(id, label, track, params.Enabled);

		const auto leaf = [this](const UIBoxParams& box)
		{
			m_Tree.BeginBox(box);
			m_Tree.EndBox();
		};
		const UISize barHeight = UISize::Fixed(UILook::k_SliderTrackHeight);
		const float barRadius = UILook::k_SliderTrackHeight * 0.5f;
		const glm::vec4 fillColor = !params.Enabled ? UILook::k_TextDisabled : ((result.Hovered || result.Down) ? UILook::k_AccentHover : UILook::k_Accent);
		if (t > 0.0f)
			leaf(Solid(UISize::Grow(t), barHeight, fillColor, barRadius));
		leaf(Solid(UISize::Fixed(UILook::k_SliderHandle), UISize::Fixed(UILook::k_SliderHandle), params.Enabled ? UILook::k_Knob : UILook::k_TextDisabled, UILook::k_SliderHandle * 0.5f));
		if (t < 1.0f)
			leaf(Solid(UISize::Grow(1.0f - t), barHeight, params.Enabled ? UILook::k_Track : UILook::k_Disabled, barRadius));
		m_Tree.EndBox();

		if (params.ShowValue)
		{
			const std::string text = integer ? std::format("{}", static_cast<long long>(current)) : (step >= 1.0 && step == std::floor(step) ? std::format("{:.0f}", current) : std::format("{:.2f}", current));
			Text(text, LabelStyle(params.Enabled).SetWidth(UISize::Fixed(56.0f)).SetAlign(UITextAlign::Right));
		}
		const std::string_view caption = VisibleLabel(label);
		if (!caption.empty())
			Text(caption, LabelStyle(params.Enabled));
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
		const float t = std::clamp(value, 0.0f, 1.0f);
		const UISize width = params.Width.Mode == UISizeMode::Fit ? UISize::Fixed(UILook::k_ProgressWidth) : params.Width;
		const UISize height = params.Height.Mode == UISizeMode::Fit ? UISize::Fixed(UILook::k_ProgressHeight) : params.Height;

		UIBoxParams bar = Solid(width, height, params.Enabled ? UILook::k_Track : UILook::k_Disabled, UILook::k_ProgressHeight * 0.5f);
		bar.Direction = UIDirection::Row;
		BeginWidgetBox(id, idText, bar, false);
		if (t > 0.0f)
			leaf(Solid(UISize::Grow(t), UISize::Grow(1.0f), params.Enabled ? UILook::k_Accent : UILook::k_TextDisabled, UILook::k_ProgressHeight * 0.5f));
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

		glm::vec4 fill = UILook::k_Transparent;
		if (!params.Enabled)
			fill = selected ? UILook::k_Disabled : UILook::k_Transparent;
		else if (selected)
			fill = result.Hovered ? UILook::k_AccentHover : UILook::k_Accent;
		else if (result.Hovered)
			fill = (result.Down) ? UILook::k_Pressed : UILook::k_Hover;

		UIBoxParams box;
		box.Width = params.Width;
		box.Height = params.Height;
		box.Padding = { UILook::k_RowPaddingX, UILook::k_RowPaddingY, UILook::k_RowPaddingX, UILook::k_RowPaddingY };
		box.Justify = UIJustify::Center;
		box.Background = fill;
		box.CornerRadius = UILook::k_CornerRadius;
		BeginWidgetBox(id, label, box, params.Enabled);
		Text(VisibleLabel(label), LabelStyle(params.Enabled));
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

		BeginWidgetBox(id, label, params.Layout, params.DragScroll, true, offset);
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
