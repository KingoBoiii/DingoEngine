#include "depch.h"
#include "DingoUIDebug.h"
#include "DingoUIContext.h"

namespace Dingo::Internal::DingoUIDebug
{

	bool Collect(Snapshot& out, bool detail)
	{
		const DingoUIContext* context = GetDingoUIContext();
		if (!context)
			return false;

		const UILayoutTree& tree = context->GetLastLayout();
		const std::vector<UIElement>& elements = tree.GetElements();
		const UIRenderStatistics& render = context->GetRenderStatistics();
		const UIFrameCosts& costs = context->GetFrameCosts();

		out.Roots.clear();
		out.Elements.clear();
		out.BlockingRects.clear();

		Stats& stats = out.Frame;
		stats = {};
		stats.Frame = context->GetFrameIndex();
		stats.Elements = static_cast<uint32_t>(elements.size());
		stats.DrawCalls = render.DrawCalls;
		stats.Quads = render.Quads;
		stats.CulledQuads = render.CulledQuads;
		stats.Vertices = render.Vertices;
		stats.Uploads = render.Uploads;
		stats.CacheHits = render.CacheHits;
		stats.UploadBytes = render.UploadBytes;
		stats.StateMapSize = static_cast<uint32_t>(context->GetStates().Size());
		stats.TweenCount = static_cast<uint32_t>(context->GetTweens().Size());
		stats.TransitionRoots = static_cast<uint32_t>(context->GetRootTransitions().Size());
		stats.ExitingRoots = costs.ExitingRoots;
		stats.IdCollisions = context->GetIdCollisionCount();
		stats.BeginMs = costs.BeginMs;
		stats.LayoutMs = costs.LayoutMs;
		stats.EmitMs = costs.EmitMs;
		stats.BuildMs = render.BuildMs;
		stats.UploadMs = render.UploadMs;
		stats.DrawMs = render.DrawMs;

		out.HoverId = context->GetHoverId();
		out.ActiveId = context->GetActiveId();
		out.FocusId = context->GetFocusId();
		out.FocusVisible = context->IsFocusVisible();
		out.Modal = tree.HasModal();
		out.CapturingInput = context->IsCapturingInput();
		out.PointerOverUI = context->IsPointerOverUI();
		out.TargetSize = context->GetTargetSize();
		const glm::vec2 toTarget = context->WindowToTarget(glm::vec2(1.0f));
		out.Origin = context->GetDebugOrigin();
		out.TargetToWindow = { toTarget.x > 0.0f ? 1.0f / toTarget.x : 1.0f, toTarget.y > 0.0f ? 1.0f / toTarget.y : 1.0f };

		std::vector<int32_t> rootOrder;
		tree.GetRootOrder(rootOrder);
		out.Roots.reserve(rootOrder.size());
		for (const int32_t index : rootOrder)
		{
			const UIElement& root = elements[index];
			RootRow row;
			row.Id = root.Id;
			row.Name = std::string(tree.GetText(root));
			row.Layer = root.Layer;
			row.World = root.World;
			row.BlocksInput = root.BlocksInput;
			row.Modal = root.Modal;
			row.Navigable = root.Navigable;
			if (const UIRootTransitions::Entry* entry = context->GetRootTransitions().Find(root.Id))
			{
				row.HasTransition = true;
				row.TransitionProgress = entry->Progress;
			}
			out.Roots.push_back(std::move(row));
		}
		stats.Roots = static_cast<uint32_t>(out.Roots.size());

		for (const UIElement& element : elements)
			stats.Widgets += (element.Interactive || element.Scroll) ? 1u : 0u;

		if (!detail)
			return true;

		std::vector<uint32_t> depths(elements.size(), 0);
		std::vector<uint8_t> blocks(elements.size(), 1);
		out.Elements.reserve(elements.size());
		for (size_t i = 0; i < elements.size(); ++i)
		{
			const UIElement& element = elements[i];
			if (element.Parent == k_UINoElement)
			{
				depths[i] = 0;
				blocks[i] = element.BlocksInput ? 1 : 0;
			}
			else
			{
				depths[i] = depths[element.Parent] + 1;
				blocks[i] = blocks[element.Parent];
			}

			ElementRow row;
			row.Id = element.Id;
			row.Depth = depths[i];
			row.Kind = static_cast<ElementKind>(element.Kind);
			if (element.Kind == UIElementKind::Text || element.Kind == UIElementKind::Root)
				row.Label = std::string(tree.GetText(element));
			row.Min = element.Rect.Min;
			row.Max = element.Rect.Max;
			row.Interactive = element.Interactive;
			row.Scroll = element.Scroll;
			row.Focusable = element.Focusable;
			row.BlocksInput = blocks[i] != 0;
			row.Hovered = element.Id != 0 && element.Id == out.HoverId;
			row.Active = element.Id != 0 && element.Id == out.ActiveId;
			row.Focused = element.Id != 0 && element.Id == out.FocusId;
			out.Elements.push_back(std::move(row));
		}

		std::vector<UIRect> rects;
		tree.CollectBlockingRects(rects);
		out.BlockingRects.reserve(rects.size());
		for (const UIRect& rect : rects)
			out.BlockingRects.emplace_back(rect.Min, rect.Max);
		return true;
	}

}
