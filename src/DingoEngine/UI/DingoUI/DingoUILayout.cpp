#include "depch.h"
#include "DingoUILayout.h"

#include "DingoEngine/Graphics/Texture.h"

#include <algorithm>
#include <cmath>

namespace Dingo::Internal
{

	namespace
	{
		constexpr uint64_t k_FnvOffset = 14695981039346656037ull;
		constexpr uint64_t k_FnvPrime = 1099511628211ull;
		constexpr float k_Epsilon = 0.01f;
		constexpr float k_ScrollThumbMargin = 2.0f;
		constexpr float k_ScrollThumbMin = 16.0f;

		uint64_t Mix(uint64_t seed, uint64_t value)
		{
			return seed ^ (value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2));
		}

		glm::vec2 AnchorFraction(UIAnchor anchor)
		{
			switch (anchor)
			{
			case UIAnchor::TopLeft: return { 0.0f, 0.0f };
			case UIAnchor::Top: return { 0.5f, 0.0f };
			case UIAnchor::TopRight: return { 1.0f, 0.0f };
			case UIAnchor::Left: return { 0.0f, 0.5f };
			case UIAnchor::Center: return { 0.5f, 0.5f };
			case UIAnchor::Right: return { 1.0f, 0.5f };
			case UIAnchor::BottomLeft: return { 0.0f, 1.0f };
			case UIAnchor::Bottom: return { 0.5f, 1.0f };
			case UIAnchor::BottomRight: return { 1.0f, 1.0f };
			case UIAnchor::Stretch: break;
			}
			return { 0.0f, 0.0f };
		}

		UIAxisSize ToPixels(const UISize& size, float scale)
		{
			UIAxisSize result;
			result.Mode = size.Mode;
			result.Value = size.Mode == UISizeMode::Fixed ? size.Value * scale : size.Value;
			result.Min = size.MinSize * scale;
			result.Max = size.MaxSize >= (std::numeric_limits<float>::max)() ? size.MaxSize : size.MaxSize * scale;
			return result;
		}

		float Snap(float value)
		{
			return std::round(value);
		}

		UIRect Intersect(const UIRect& a, const UIRect& b)
		{
			UIRect result;
			result.Min = glm::max(a.Min, b.Min);
			result.Max = glm::max(glm::min(a.Max, b.Max), result.Min);
			return result;
		}

		bool Covers(const UIElement& element);

		bool Draws(const UIElement& element)
		{
			switch (element.Kind)
			{
			case UIElementKind::Text:
			case UIElementKind::Image:
				return true;
			case UIElementKind::Root:
			case UIElementKind::Box:
				return element.Background.a > 0.0f || (element.BorderWidth > 0.0f && element.BorderColor.a > 0.0f) || element.NineSlice.IsValid();
			}
			return false;
		}

		bool Covers(const UIElement& element)
		{
			return element.Interactive || element.Scroll || Draws(element);
		}

		bool IsContainer(const UIElement& element)
		{
			return element.Kind == UIElementKind::Root || element.Kind == UIElementKind::Box;
		}

		bool IsWrapText(const UIElement& element)
		{
			return element.Kind == UIElementKind::Text && element.Wrap;
		}
	}

	float ComputeUIScale(const UIScale& scale, const glm::vec2& viewport)
	{
		if (scale.ScaleKind == UIScale::Kind::ConstantPixel)
			return scale.Factor > 0.0f ? scale.Factor : 1.0f;

		if (scale.Reference.x <= 0.0f || scale.Reference.y <= 0.0f || viewport.x <= 0.0f || viewport.y <= 0.0f)
			return 1.0f;

		const float match = std::clamp(scale.Match, 0.0f, 1.0f);
		const float logWidth = std::log2(viewport.x / scale.Reference.x);
		const float logHeight = std::log2(viewport.y / scale.Reference.y);
		return std::exp2(logWidth + (logHeight - logWidth) * match);
	}

	uint64_t UILayoutTree::HashName(std::string_view name)
	{
		uint64_t hash = k_FnvOffset;
		for (const char character : name)
		{
			hash ^= static_cast<uint8_t>(character);
			hash *= k_FnvPrime;
		}
		return hash;
	}

	void UILayoutTree::Begin(const glm::vec2& viewport)
	{
		m_Elements.clear();
		m_Open.clear();
		m_IdStack.clear();
		m_OpenIdDepth.clear();
		m_Text.clear();
		m_Viewport = viewport;
		m_RootScale = 1.0f;
	}

	int32_t UILayoutTree::Append(UIElementKind kind)
	{
		const int32_t index = static_cast<int32_t>(m_Elements.size());
		UIElement& element = m_Elements.emplace_back();
		element.Kind = kind;
		element.Scale = m_RootScale;

		if (!m_Open.empty())
		{
			const int32_t parentIndex = m_Open.back();
			UIElement& parent = m_Elements[parentIndex];
			element.Parent = parentIndex;
			if (parent.LastChild == k_UINoElement)
				parent.FirstChild = index;
			else
				m_Elements[parent.LastChild].NextSibling = index;
			parent.LastChild = index;
			element.Id = Mix(parent.Id, Mix(static_cast<uint64_t>(kind) + 1, parent.ChildCount));
			++parent.ChildCount;
		}
		return index;
	}

	void UILayoutTree::ApplyBox(UIElement& element, const UIBoxParams& params)
	{
		const float scale = m_RootScale;
		element.Size[0] = ToPixels(params.Width, scale);
		element.Size[1] = ToPixels(params.Height, scale);
		element.Horizontal = params.Direction == UIDirection::Row;
		element.Gap = params.Gap * scale;
		element.Padding = params.Padding * scale;
		element.Justify = params.Justify;
		element.Align = params.Align;
		element.Background = params.Background;
		element.BorderColor = params.BorderColor;
		element.BorderWidth = params.BorderWidth * scale;
		element.CornerRadius = params.CornerRadius * scale;
		element.ClipChildren = params.ClipChildren;
		element.NineSlice = params.NineSlice;

		if (!params.Id.empty())
		{
			element.NameHash = HashName(params.Id);
			element.Id = Mix(element.Parent == k_UINoElement ? 0 : m_Elements[element.Parent].Id, element.NameHash);
		}
	}

	bool UILayoutTree::BeginRoot(std::string_view name, const UIRootParams& params)
	{
		return BeginRootScaled(name, params, 1.0f);
	}

	bool UILayoutTree::BeginRootScaled(std::string_view name, const UIRootParams& params, float scaleMultiplier)
	{
		if (!m_Open.empty())
			return false;

		m_RootScale = ComputeUIScale(params.Scale, m_Viewport) * scaleMultiplier;
		const int32_t index = Append(UIElementKind::Root);
		UIElement& element = m_Elements[index];
		ApplyBox(element, params.Layout);
		element.NameHash = HashName(name);
		element.Id = Mix(k_FnvOffset, element.NameHash);
		element.TextOffset = static_cast<uint32_t>(m_Text.size());
		element.TextLength = static_cast<uint32_t>(name.size());
		m_Text.append(name);
		element.Anchor = params.Anchor;
		element.Offset = params.Offset * m_RootScale;
		element.Pivot = params.HasPivot ? params.Pivot : AnchorFraction(params.Anchor);
		element.Layer = params.Layer;
		element.BlocksInput = params.BlocksInput;
		element.Modal = params.Modal;
		element.Navigable = params.Navigable || params.Modal;
		element.Transition = params.Transition;
		m_Open.push_back(index);
		m_OpenIdDepth.push_back(static_cast<uint32_t>(m_IdStack.size()));
		return true;
	}

	bool UILayoutTree::BeginWorldRoot(std::string_view name, const UIWorldRootParams& params, const glm::vec2& point, float depth, float distanceScale, float alpha)
	{
		UIRootParams root;
		root.Anchor = UIAnchor::TopLeft;
		root.Scale = params.Scale;
		root.Layer = params.Layer;
		root.BlocksInput = params.BlocksInput;
		root.Navigable = params.Navigable && params.BlocksInput;
		root.Layout = params.Layout;
		if (!BeginRootScaled(name, root, distanceScale))
			return false;

		UIElement& element = m_Elements.back();
		element.World = true;
		element.Depth = depth;
		element.Alpha = alpha;
		element.Pivot = params.Pivot;
		element.Offset = point + params.Offset * m_RootScale;
		return true;
	}

	bool UILayoutTree::BeginBox(const UIBoxParams& params, const UIWidgetInfo* widget)
	{
		if (m_Open.empty())
			return false;

		const int32_t index = Append(UIElementKind::Box);
		UIElement& element = m_Elements[index];
		ApplyBox(element, params);
		if (widget)
		{
			element.Id = widget->Id;
			element.NameHash = widget->NameHash;
			element.Interactive = widget->Interactive;
			element.State = widget->State;
			element.Scroll = widget->Scroll;
			element.Focusable = widget->Focusable;
			element.DefaultFocus = widget->DefaultFocus;
			element.AdjustsHorizontally = widget->AdjustsHorizontally;
			element.ScrollOffset = widget->ScrollOffset;
			element.ScrollThumbColor = widget->ScrollThumbColor;
			element.ScrollThumbWidth = widget->ScrollThumbWidth;
			if (widget->Scroll)
				element.ClipChildren = true;
		}
		m_Open.push_back(index);
		m_OpenIdDepth.push_back(static_cast<uint32_t>(m_IdStack.size()));
		return true;
	}

	bool UILayoutTree::AddText(std::string_view text, const UITextParams& params, const Font* font)
	{
		if (m_Open.empty())
			return false;

		const int32_t index = Append(UIElementKind::Text);
		UIElement& element = m_Elements[index];
		element.Size[0] = ToPixels(params.Width, m_RootScale);
		element.TextFont = font;
		element.TextOffset = static_cast<uint32_t>(m_Text.size());
		element.TextLength = static_cast<uint32_t>(text.size());
		m_Text.append(text);
		element.TextSize = params.Size * m_RootScale;
		element.TextColor = params.Color;
		element.TextAlign = params.Align;
		element.Wrap = params.Wrap;
		element.MaxWidth = params.MaxWidth * m_RootScale;
		return true;
	}

	bool UILayoutTree::AddImage(Texture* texture, const glm::vec2& naturalSize, const UIImageParams& params)
	{
		if (m_Open.empty())
			return false;

		const int32_t index = Append(UIElementKind::Image);
		UIElement& element = m_Elements[index];
		element.Size[0] = ToPixels(params.Width, m_RootScale);
		element.Size[1] = ToPixels(params.Height, m_RootScale);
		element.ImageTexture = texture;
		element.ImageSize = naturalSize * m_RootScale;
		element.UVMin = params.UVMin;
		element.UVMax = params.UVMax;
		element.Tint = params.Tint;
		element.PreserveAspect = params.PreserveAspect;
		return true;
	}

	void UILayoutTree::CloseElement()
	{
		m_IdStack.resize((std::min)(m_IdStack.size(), static_cast<size_t>(m_OpenIdDepth.back())));
		m_OpenIdDepth.pop_back();
		m_Open.pop_back();
	}

	bool UILayoutTree::EndRoot()
	{
		if (m_Open.empty() || m_Elements[m_Open.back()].Kind != UIElementKind::Root)
			return false;
		CloseElement();
		return true;
	}

	bool UILayoutTree::EndBox()
	{
		if (m_Open.empty() || m_Elements[m_Open.back()].Kind != UIElementKind::Box)
			return false;
		CloseElement();
		return true;
	}

	uint32_t UILayoutTree::CloseAll()
	{
		const uint32_t count = static_cast<uint32_t>(m_Open.size());
		m_Open.clear();
		m_OpenIdDepth.clear();
		m_IdStack.clear();
		return count;
	}

	uint64_t UILayoutTree::MakeId(std::string_view label) const
	{
		if (m_Open.empty())
			return 0;
		const uint64_t pushed = m_IdStack.empty() ? 0 : m_IdStack.back();
		const uint64_t id = Mix(Mix(m_Elements[m_Open.back()].Id, pushed), HashName(label));
		return id != 0 ? id : 1;
	}

	void UILayoutTree::PushId(std::string_view id)
	{
		const uint64_t pushed = m_IdStack.empty() ? 0 : m_IdStack.back();
		m_IdStack.push_back(Mix(pushed, HashName(id)));
	}

	void UILayoutTree::PushId(int id)
	{
		const uint64_t pushed = m_IdStack.empty() ? 0 : m_IdStack.back();
		m_IdStack.push_back(Mix(pushed, static_cast<uint64_t>(static_cast<int64_t>(id)) + 0x51ed270b3ull));
	}

	bool UILayoutTree::PopId()
	{
		const size_t base = m_OpenIdDepth.empty() ? 0 : m_OpenIdDepth.back();
		if (m_IdStack.size() <= base)
			return false;
		m_IdStack.pop_back();
		return true;
	}

	float UILayoutTree::Clamp(const UIAxisSize& size, float value)
	{
		return (std::max)(size.Min, (std::min)(size.Max, value));
	}

	float UILayoutTree::ContentOf(const UIElement& child, uint32_t axis) const
	{
		const UIAxisSize& size = child.Size[axis];
		switch (size.Mode)
		{
		case UISizeMode::Fixed:
			return Clamp(size, size.Value);
		case UISizeMode::Percent:
			return Clamp(size, 0.0f);
		case UISizeMode::Fit:
		case UISizeMode::Grow:
			break;
		}
		return Clamp(size, child.Natural[axis]);
	}

	void UILayoutTree::MeasureText(UIElement& element, float maxWidth)
	{
		const glm::vec2 measured = (*m_Measure)(element.TextFont, GetText(element), element.TextSize, maxWidth);
		element.Natural[0] = measured.x;
		element.Natural[1] = measured.y;
		element.WrapWidth = maxWidth;
	}

	void UILayoutTree::ComputeMinContent(UIElement& element)
	{
		const std::string_view text = GetText(element);
		float widest = 0.0f;
		size_t begin = 0;
		while (begin <= text.size())
		{
			size_t end = text.find_first_of(" \n", begin);
			if (end == std::string_view::npos)
				end = text.size();
			if (end > begin)
				widest = (std::max)(widest, (*m_Measure)(element.TextFont, text.substr(begin, end - begin), element.TextSize, 0.0f).x);
			begin = end + 1;
		}
		element.MinContentWidth = widest;
	}

	void UILayoutTree::FitAxis(uint32_t axis)
	{
		for (int32_t i = static_cast<int32_t>(m_Elements.size()) - 1; i >= 0; --i)
		{
			UIElement& element = m_Elements[i];
			switch (element.Kind)
			{
			case UIElementKind::Text:
				if (axis == 0)
					MeasureText(element, element.Wrap ? element.MaxWidth : 0.0f);
				break;

			case UIElementKind::Image:
			{
				const bool aspect = element.PreserveAspect && element.ImageSize.x > 0.0f && element.ImageSize.y > 0.0f;
				if (axis == 0)
				{
					element.Natural[0] = element.ImageSize.x;
					if (aspect && element.Size[0].Mode == UISizeMode::Fit && element.Size[1].Mode == UISizeMode::Fixed)
						element.Natural[0] = Clamp(element.Size[1], element.Size[1].Value) * element.ImageSize.x / element.ImageSize.y;
				}
				else
				{
					element.Natural[1] = element.ImageSize.y;
					if (aspect && element.Size[1].Mode == UISizeMode::Fit)
						element.Natural[1] = element.Resolved[0] * element.ImageSize.y / element.ImageSize.x;
				}
				break;
			}

			case UIElementKind::Root:
			case UIElementKind::Box:
			{
				const bool main = (axis == 0) == element.Horizontal;
				const float padding = axis == 0 ? element.Padding.x + element.Padding.z : element.Padding.y + element.Padding.w;
				float content = 0.0f;
				uint32_t count = 0;
				for (int32_t child = element.FirstChild; child != k_UINoElement; child = m_Elements[child].NextSibling)
				{
					const float size = ContentOf(m_Elements[child], axis);
					content = main ? content + size : (std::max)(content, size);
					++count;
				}
				if (main && count > 1)
					content += element.Gap * static_cast<float>(count - 1);
				element.Natural[axis] = padding + content;
				break;
			}
			}
		}
	}

	void UILayoutTree::ResolveAxis(uint32_t axis)
	{
		for (size_t i = 0; i < m_Elements.size(); ++i)
		{
			UIElement& element = m_Elements[i];

			if (element.Kind == UIElementKind::Root)
			{
				const UIAxisSize& size = element.Size[axis];
				float value = element.Anchor == UIAnchor::Stretch ? m_Viewport[axis] : 0.0f;
				if (element.Anchor != UIAnchor::Stretch)
				{
					switch (size.Mode)
					{
					case UISizeMode::Fixed: value = size.Value; break;
					case UISizeMode::Fit: value = element.Natural[axis]; break;
					case UISizeMode::Grow: value = m_Viewport[axis]; break;
					case UISizeMode::Percent: value = size.Value * m_Viewport[axis]; break;
					}
				}
				element.Resolved[axis] = Clamp(size, value);
			}
			else if (axis == 0 && IsWrapText(element) && element.Resolved[0] > 0.0f && element.Resolved[0] < element.Natural[0] - k_Epsilon)
			{
				MeasureText(element, element.Resolved[0]);
			}

			if (!IsContainer(element) || element.FirstChild == k_UINoElement)
				continue;

			const bool main = (axis == 0) == element.Horizontal;
			const float padding = axis == 0 ? element.Padding.x + element.Padding.z : element.Padding.y + element.Padding.w;
			const float inner = (std::max)(0.0f, element.Resolved[axis] - padding);

			if (!main)
			{
				for (int32_t child = element.FirstChild; child != k_UINoElement; child = m_Elements[child].NextSibling)
				{
					UIElement& item = m_Elements[child];
					const UIAxisSize& size = item.Size[axis];
					float value = 0.0f;
					switch (size.Mode)
					{
					case UISizeMode::Fixed: value = size.Value; break;
					case UISizeMode::Percent: value = size.Value * inner; break;
					case UISizeMode::Grow: value = inner; break;
					case UISizeMode::Fit:
						value = item.Natural[axis];
						if (element.Align == UIAlign::Stretch || (axis == 0 && IsWrapText(item) && value > inner))
							value = inner;
						break;
					}
					item.Resolved[axis] = Clamp(size, value);
				}
				continue;
			}

			uint32_t count = 0;
			float fixedTotal = 0.0f;
			float weightTotal = 0.0f;
			for (int32_t child = element.FirstChild; child != k_UINoElement; child = m_Elements[child].NextSibling)
			{
				UIElement& item = m_Elements[child];
				const UIAxisSize& size = item.Size[axis];
				++count;
				switch (size.Mode)
				{
				case UISizeMode::Fixed: item.Resolved[axis] = Clamp(size, size.Value); break;
				case UISizeMode::Percent: item.Resolved[axis] = Clamp(size, size.Value * inner); break;
				case UISizeMode::Fit: item.Resolved[axis] = Clamp(size, item.Natural[axis]); break;
				case UISizeMode::Grow:
					item.Resolved[axis] = 0.0f;
					weightTotal += (std::max)(size.Value, 0.0f);
					continue;
				}
				fixedTotal += item.Resolved[axis];
			}

			const float gaps = element.Gap * static_cast<float>(count - 1);

			if (weightTotal > 0.0f)
			{
				float remaining = inner - gaps - fixedTotal;
				float weights = weightTotal;
				for (int32_t child = element.FirstChild; child != k_UINoElement; child = m_Elements[child].NextSibling)
				{
					UIElement& item = m_Elements[child];
					if (item.Size[axis].Mode == UISizeMode::Grow)
						item.Resolved[axis] = -1.0f;
				}

				// A grow element that hits its min or max is frozen at it and the rest share what is left.
				for (uint32_t pass = 0; pass <= count; ++pass)
				{
					bool frozeAny = false;
					for (int32_t child = element.FirstChild; child != k_UINoElement; child = m_Elements[child].NextSibling)
					{
						UIElement& item = m_Elements[child];
						const UIAxisSize& size = item.Size[axis];
						if (size.Mode != UISizeMode::Grow || item.Resolved[axis] >= 0.0f)
							continue;

						const float share = weights > 0.0f ? (std::max)(remaining, 0.0f) * (std::max)(size.Value, 0.0f) / weights : 0.0f;
						const float clamped = Clamp(size, share);
						if (clamped != share)
						{
							item.Resolved[axis] = clamped;
							remaining -= clamped;
							weights -= (std::max)(size.Value, 0.0f);
							frozeAny = true;
						}
					}
					if (!frozeAny)
						break;
				}

				for (int32_t child = element.FirstChild; child != k_UINoElement; child = m_Elements[child].NextSibling)
				{
					UIElement& item = m_Elements[child];
					const UIAxisSize& size = item.Size[axis];
					if (size.Mode != UISizeMode::Grow || item.Resolved[axis] >= 0.0f)
						continue;
					const float share = weights > 0.0f ? (std::max)(remaining, 0.0f) * (std::max)(size.Value, 0.0f) / weights : 0.0f;
					item.Resolved[axis] = Clamp(size, share);
				}
			}
			else if (axis == 0 && fixedTotal + gaps > inner + k_Epsilon)
			{
				const float overflow = fixedTotal + gaps - inner;
				float capacity = 0.0f;
				for (int32_t child = element.FirstChild; child != k_UINoElement; child = m_Elements[child].NextSibling)
				{
					UIElement& item = m_Elements[child];
					if (!IsWrapText(item) || item.Size[0].Mode != UISizeMode::Fit)
						continue;
					if (item.MinContentWidth < 0.0f)
						ComputeMinContent(item);
					capacity += (std::max)(0.0f, item.Resolved[0] - item.MinContentWidth);
				}
				if (capacity > 0.0f)
				{
					const float ratio = (std::min)(1.0f, overflow / capacity);
					for (int32_t child = element.FirstChild; child != k_UINoElement; child = m_Elements[child].NextSibling)
					{
						UIElement& item = m_Elements[child];
						if (!IsWrapText(item) || item.Size[0].Mode != UISizeMode::Fit)
							continue;
						item.Resolved[0] -= (std::max)(0.0f, item.Resolved[0] - item.MinContentWidth) * ratio;
					}
				}
			}
		}
	}

	void UILayoutTree::Place()
	{
		for (size_t i = 0; i < m_Elements.size(); ++i)
		{
			UIElement& element = m_Elements[i];

			if (element.Kind == UIElementKind::Root)
			{
				const glm::vec2 size = { element.Resolved[0], element.Resolved[1] };
				const glm::vec2 anchor = element.Anchor == UIAnchor::Stretch ? glm::vec2(0.0f) : AnchorFraction(element.Anchor) * m_Viewport;
				element.Origin = anchor - element.Pivot * size + element.Offset;
				element.Clip = k_UINoClip;
			}
			else
			{
				const UIElement& parent = m_Elements[element.Parent];
				element.Clip = parent.ClipChildren ? Intersect(parent.Clip, parent.Rect) : parent.Clip;
			}

			element.Rect.Min = { Snap(element.Origin.x), Snap(element.Origin.y) };
			element.Rect.Max = { (std::max)(element.Rect.Min.x, Snap(element.Origin.x + element.Resolved[0])), (std::max)(element.Rect.Min.y, Snap(element.Origin.y + element.Resolved[1])) };

			if (!IsContainer(element) || element.FirstChild == k_UINoElement)
				continue;

			const uint32_t mainAxis = element.Horizontal ? 0 : 1;
			const uint32_t crossAxis = 1 - mainAxis;
			const glm::vec2 padMin = { element.Padding.x, element.Padding.y };
			const glm::vec2 padMax = { element.Padding.z, element.Padding.w };
			const glm::vec2 inner = glm::max(glm::vec2(element.Resolved[0], element.Resolved[1]) - padMin - padMax, glm::vec2(0.0f));

			uint32_t count = 0;
			float total = 0.0f;
			float widest = 0.0f;
			for (int32_t child = element.FirstChild; child != k_UINoElement; child = m_Elements[child].NextSibling)
			{
				total += m_Elements[child].Resolved[mainAxis];
				widest = (std::max)(widest, m_Elements[child].Resolved[crossAxis]);
				++count;
			}
			total += element.Gap * static_cast<float>(count - 1);
			element.ContentSize[mainAxis] = total + padMin[mainAxis] + padMax[mainAxis];
			element.ContentSize[crossAxis] = widest + padMin[crossAxis] + padMax[crossAxis];

			const float free = (std::max)(0.0f, inner[mainAxis] - total);
			float cursor = 0.0f;
			float extraGap = 0.0f;
			switch (element.Justify)
			{
			case UIJustify::Start: break;
			case UIJustify::Center: cursor = free * 0.5f; break;
			case UIJustify::End: cursor = free; break;
			case UIJustify::SpaceBetween:
				if (count > 1)
					extraGap = free / static_cast<float>(count - 1);
				break;
			}

			for (int32_t child = element.FirstChild; child != k_UINoElement; child = m_Elements[child].NextSibling)
			{
				UIElement& item = m_Elements[child];
				float cross = 0.0f;
				const float spare = inner[crossAxis] - item.Resolved[crossAxis];
				if (element.Align == UIAlign::Center)
					cross = spare * 0.5f;
				else if (element.Align == UIAlign::End)
					cross = spare;

				glm::vec2 offset;
				offset[mainAxis] = cursor;
				offset[crossAxis] = cross;
				item.Origin = element.Origin + padMin + offset - (element.Scroll ? element.ScrollOffset : glm::vec2(0.0f));
				cursor += item.Resolved[mainAxis] + element.Gap + extraGap;
			}
		}
	}

	void UILayoutTree::Layout(const UITextMeasureFn& measure)
	{
		DE_PROFILE_FUNCTION();

		if (m_Elements.empty())
			return;

		m_Measure = &measure;
		for (UIElement& element : m_Elements)
			element.MinContentWidth = -1.0f;

		FitAxis(0);
		ResolveAxis(0);
		FitAxis(1);
		ResolveAxis(1);
		Place();
		m_Measure = nullptr;
	}

	void UILayoutTree::SortRoots() const
	{
		m_Order.clear();
		for (size_t i = 0; i < m_Elements.size(); ++i)
		{
			if (m_Elements[i].Kind == UIElementKind::Root)
				m_Order.push_back(static_cast<int32_t>(i));
		}
		std::stable_sort(m_Order.begin(), m_Order.end(), [this](int32_t a, int32_t b)
		{
			const UIElement& left = m_Elements[a];
			const UIElement& right = m_Elements[b];
			if (left.World != right.World)
				return left.World;
			if (left.Layer != right.Layer)
				return left.Layer < right.Layer;
			return left.World && left.Depth > right.Depth;
		});
	}

	void UILayoutTree::Emit(UIDrawList& list) const
	{
		DE_PROFILE_FUNCTION();

		if (m_Elements.empty())
			return;

		SortRoots();
		const std::vector<int32_t> roots = m_Order;
		for (const int32_t root : roots)
			EmitRoot(root, list);
	}

	void UILayoutTree::EmitRoot(int32_t root, UIDrawList& list) const
	{
		const int32_t elementCount = static_cast<int32_t>(m_Elements.size());
		{
			int32_t end = root + 1;
			while (end < elementCount && m_Elements[end].Kind != UIElementKind::Root)
				++end;

			for (int32_t i = root; i < end; ++i)
			{
				const UIElement& element = m_Elements[i];
				switch (element.Kind)
				{
				case UIElementKind::Root:
				case UIElementKind::Box:
					if (element.Background.a > 0.0f || (element.BorderWidth > 0.0f && element.BorderColor.a > 0.0f))
						list.AddRect(element.Rect, element.Background, element.CornerRadius, element.BorderWidth, element.BorderColor, element.Clip);
					if (element.NineSlice.IsValid())
						list.AddNineSlice(element.Rect, element.NineSlice.Image, glm::vec2(static_cast<float>(element.NineSlice.Image->GetWidth()), static_cast<float>(element.NineSlice.Image->GetHeight())), element.NineSlice.Borders, element.NineSlice.Scale * element.Scale, element.NineSlice.FillCenter, element.NineSlice.PointFilter, element.NineSlice.Tint, element.Clip);
					break;

				case UIElementKind::Text:
					if (element.TextFont)
					{
						const float align = element.TextAlign == UITextAlign::Center ? 0.5f : (element.TextAlign == UITextAlign::Right ? 1.0f : 0.0f);
						list.AddText(element.Rect.Min, element.TextFont, GetText(element), element.TextSize, element.TextColor, element.WrapWidth, element.Clip, align, element.Rect.Max.x - element.Rect.Min.x);
					}
					break;

				case UIElementKind::Image:
				{
					UIRect rect = element.Rect;
					if (element.PreserveAspect && element.ImageSize.x > 0.0f && element.ImageSize.y > 0.0f && !rect.IsEmpty())
					{
						const glm::vec2 box = rect.Max - rect.Min;
						const float fit = (std::min)(box.x / element.ImageSize.x, box.y / element.ImageSize.y);
						const glm::vec2 drawn = element.ImageSize * fit;
						rect.Min = { Snap(rect.Min.x + (box.x - drawn.x) * 0.5f), Snap(rect.Min.y + (box.y - drawn.y) * 0.5f) };
						rect.Max = { (std::max)(rect.Min.x, Snap(rect.Min.x + drawn.x)), (std::max)(rect.Min.y, Snap(rect.Min.y + drawn.y)) };
					}
					list.AddImage(rect, element.ImageTexture, element.UVMin, element.UVMax, element.Tint, element.Clip);
					break;
				}
				}
			}

			for (int32_t i = root; i < end; ++i)
			{
				const UIElement& element = m_Elements[i];
				const float height = element.Rect.Max.y - element.Rect.Min.y;
				const float range = element.ContentSize.y - height;
				if (!element.Scroll || range <= 0.5f || height <= 0.0f)
					continue;

				const float width = element.ScrollThumbWidth * element.Scale;
				const float margin = k_ScrollThumbMargin * element.Scale;
				const float thumb = (std::min)(height, (std::max)(k_ScrollThumbMin * element.Scale, height * height / element.ContentSize.y));
				const float travel = (height - thumb) * std::clamp(element.ScrollOffset.y / range, 0.0f, 1.0f);
				UIRect rect;
				rect.Min = { Snap(element.Rect.Max.x - margin - width), Snap(element.Rect.Min.y + travel) };
				rect.Max = { Snap(element.Rect.Max.x - margin), Snap(element.Rect.Min.y + travel + thumb) };
				list.AddRect(rect, element.ScrollThumbColor, width * 0.5f, 0.0f, glm::vec4(0.0f), element.Clip);
			}
		}
	}

	const UIElement* UILayoutTree::FindById(uint64_t id) const
	{
		for (const UIElement& element : m_Elements)
		{
			if (element.Id == id)
				return &element;
		}
		return nullptr;
	}

	bool UILayoutTree::HasModal() const
	{
		for (const UIElement& element : m_Elements)
		{
			if (element.Kind == UIElementKind::Root && element.Modal)
				return true;
		}
		return false;
	}

	void UILayoutTree::GetRootOrder(std::vector<int32_t>& order) const
	{
		SortRoots();
		order = m_Order;
	}

	UIHit UILayoutTree::HitTest(const glm::vec2& point) const
	{
		UIHit hit;
		if (m_Elements.empty())
			return hit;

		SortRoots();
		const int32_t elementCount = static_cast<int32_t>(m_Elements.size());
		bool resolved = false;
		for (auto it = m_Order.rbegin(); it != m_Order.rend(); ++it)
		{
			const int32_t root = *it;
			const UIElement& rootElement = m_Elements[root];
			int32_t end = root + 1;
			while (end < elementCount && m_Elements[end].Kind != UIElementKind::Root)
				++end;

			int32_t hover = k_UINoElement;
			int32_t scroll = k_UINoElement;
			bool covers = false;
			for (int32_t i = root; i < end; ++i)
			{
				const UIElement& element = m_Elements[i];
				if (!Covers(element) || !Intersect(element.Rect, element.Clip).Contains(point))
					continue;
				covers = true;
				if (element.Interactive)
					hover = i;
				if (element.Scroll)
					scroll = i;
			}

			if (rootElement.BlocksInput && covers)
				hit.OverBlocking = true;

			if (!resolved)
			{
				if (hover != k_UINoElement)
					hit.HoverId = m_Elements[hover].Id;
				if (scroll != k_UINoElement)
					hit.ScrollId = m_Elements[scroll].Id;
				resolved = hover != k_UINoElement || scroll != k_UINoElement || (rootElement.BlocksInput && covers);
			}

			if (rootElement.Modal)
			{
				hit.Modal = true;
				hit.OverBlocking = true;
				break;
			}
		}
		return hit;
	}

	const UIElement* UILayoutTree::FindByName(std::string_view name) const
	{
		const uint64_t hash = HashName(name);
		for (const UIElement& element : m_Elements)
		{
			if (element.NameHash == hash)
				return &element;
		}
		return nullptr;
	}

	void UILayoutTree::CollectBlockingRects(std::vector<UIRect>& rects) const
	{
		rects.clear();
		const UIElement* root = nullptr;
		for (const UIElement& element : m_Elements)
		{
			if (element.Kind == UIElementKind::Root)
				root = &element;
			if (root && root->BlocksInput && Covers(element))
			{
				const UIRect rect = Intersect(element.Rect, element.Clip);
				if (!rect.IsEmpty())
					rects.push_back(rect);
			}
		}
	}

}
