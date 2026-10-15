#include "depch.h"
#include "DingoUITween.h"

#include <algorithm>
#include <cmath>

namespace Dingo::Internal
{

	float EvaluateEase(UIEase ease, float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		if (t == 0.0f || t == 1.0f)
			return t;

		constexpr float k_Pi = 3.14159265358979f;
		switch (ease)
		{
		case UIEase::Linear: return t;
		case UIEase::QuadIn: return t * t;
		case UIEase::QuadOut: return 1.0f - (1.0f - t) * (1.0f - t);
		case UIEase::QuadInOut: return t < 0.5f ? 2.0f * t * t : 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
		case UIEase::CubicIn: return t * t * t;
		case UIEase::CubicOut:
		{
			const float u = 1.0f - t;
			return 1.0f - u * u * u;
		}
		case UIEase::CubicInOut:
		{
			const float u = 1.0f - t;
			return t < 0.5f ? 4.0f * t * t * t : 1.0f - 4.0f * u * u * u;
		}
		case UIEase::BackOut:
		{
			constexpr float c1 = 1.70158f;
			constexpr float c3 = c1 + 1.0f;
			const float u = t - 1.0f;
			return 1.0f + c3 * u * u * u + c1 * u * u;
		}
		case UIEase::ElasticOut:
		{
			constexpr float c4 = 2.0f * k_Pi / 3.0f;
			return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
		}
		}
		return t;
	}

	float StepProgress(float progress, float dt, float duration, bool forward)
	{
		if (duration <= 0.0f)
			return forward ? 1.0f : 0.0f;
		const float step = (std::max)(dt, 0.0f) / duration;
		return std::clamp(forward ? progress + step : progress - step, 0.0f, 1.0f);
	}

	glm::vec4 UITweenMap::Update(uint64_t id, const glm::vec4& target, float dt, float duration, UIEase ease, uint64_t frame)
	{
		const auto [it, inserted] = m_Tweens.try_emplace(id);
		Tween& tween = it->second;
		tween.LastSeenFrame = frame;
		if (inserted)
		{
			tween.From = tween.To = tween.Value = target;
			tween.LastStepFrame = frame;
			return target;
		}

		if (tween.LastStepFrame == frame)
			return tween.Value;
		tween.LastStepFrame = frame;

		if (target != tween.To)
		{
			tween.From = tween.Value;
			tween.To = target;
			tween.Elapsed = 0.0f;
			if (duration > 0.0f)
				return tween.Value;
		}
		else if (tween.Value == tween.To)
			return tween.Value;
		else
			tween.Elapsed += (std::max)(dt, 0.0f);

		const float t = duration > 0.0f ? (std::min)(tween.Elapsed / duration, 1.0f) : 1.0f;
		tween.Value = t >= 1.0f ? tween.To : glm::mix(tween.From, tween.To, EvaluateEase(ease, t));
		return tween.Value;
	}

	size_t UITweenMap::Evict(uint64_t frame)
	{
		size_t removed = 0;
		for (auto it = m_Tweens.begin(); it != m_Tweens.end();)
		{
			if (frame >= it->second.LastSeenFrame + k_MaxAge)
			{
				it = m_Tweens.erase(it);
				++removed;
			}
			else
				++it;
		}
		return removed;
	}

	UIRootTransitions::Entry* UIRootTransitions::Declare(uint64_t id, const UITransition& transition, int layer, uint64_t frame, float dt)
	{
		if (!transition.IsActive())
		{
			m_Entries.erase(id);
			return nullptr;
		}

		const auto [it, inserted] = m_Entries.try_emplace(id);
		Entry& entry = it->second;
		if (inserted)
			entry.Progress = transition.Duration > 0.0f ? 0.0f : 1.0f;
		else if (entry.LastSeenFrame != frame)
			entry.Progress = StepProgress(entry.Progress, dt, transition.Duration, true);
		entry.Transition = transition;
		entry.Layer = layer;
		entry.Exiting = false;
		entry.LastSeenFrame = frame;
		return &entry;
	}

	const UIRootTransitions::Entry* UIRootTransitions::Find(uint64_t id) const
	{
		const auto it = m_Entries.find(id);
		return it == m_Entries.end() ? nullptr : &it->second;
	}

	void UIRootTransitions::AdvanceExiting(uint64_t frame, float dt, std::vector<Entry*>& exiting)
	{
		exiting.clear();
		for (auto it = m_Entries.begin(); it != m_Entries.end();)
		{
			Entry& entry = it->second;
			if (entry.LastSeenFrame == frame)
			{
				++it;
				continue;
			}

			const float duration = entry.Transition.ExitDuration < 0.0f ? entry.Transition.Duration : entry.Transition.ExitDuration;
			entry.Exiting = true;
			entry.Progress = StepProgress(entry.Progress, dt, duration, false);
			if (entry.Progress <= 0.0f || entry.Kept.IsEmpty())
			{
				it = m_Entries.erase(it);
				continue;
			}
			exiting.push_back(&entry);
			++it;
		}
		std::stable_sort(exiting.begin(), exiting.end(), [](const Entry* a, const Entry* b) { return a->Layer < b->Layer; });
	}

	void UIRootTransitions::ForgetTexture(const Texture* texture)
	{
		for (auto it = m_Entries.begin(); it != m_Entries.end();)
		{
			Entry& entry = it->second;
			if (!entry.Kept.References(texture))
			{
				++it;
				continue;
			}
			if (entry.Exiting)
			{
				it = m_Entries.erase(it);
				continue;
			}
			entry.Kept.Clear();
			++it;
		}
	}

	UIRootVisual UIRootTransitions::VisualOf(const Entry& entry)
	{
		const float shown = EvaluateEase(entry.Transition.Ease, entry.Progress);
		UIRootVisual visual;
		if (entry.Transition.FadeEnabled)
			visual.Alpha = std::clamp(shown, 0.0f, 1.0f);
		visual.Offset = entry.Transition.SlideOffset * (1.0f - shown);
		visual.Scale = entry.Transition.ScaleFrom + (1.0f - entry.Transition.ScaleFrom) * shown;
		return visual;
	}

}
