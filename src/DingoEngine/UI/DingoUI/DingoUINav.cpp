#include "depch.h"
#include "DingoUINav.h"

#include "DingoEngine/Core/InputBackend.h"

#include <algorithm>
#include <cmath>

namespace Dingo::Internal
{

	namespace
	{
		constexpr float k_MinStep = 0.5f;

		glm::vec2 CenterOf(const UIRect& rect)
		{
			return (rect.Min + rect.Max) * 0.5f;
		}

		bool AnyDown(const std::vector<KeyCode>& keys)
		{
			return std::any_of(keys.begin(), keys.end(), [](KeyCode key) { return InputBackend::IsKeyDownUnconsumed(key); });
		}

		bool AnyPressed(const std::vector<KeyCode>& keys)
		{
			return std::any_of(keys.begin(), keys.end(), [](KeyCode key) { return InputBackend::IsKeyPressedUnconsumed(key); });
		}

		void ConsumeKeys(const std::vector<KeyCode>& keys)
		{
			for (const KeyCode key : keys)
				InputBackend::ConsumeKey(key);
		}

		const std::vector<KeyCode>& KeysOf(const UINavParams& params, size_t direction)
		{
			switch (static_cast<UINavDirection>(direction))
			{
			case UINavDirection::Left: return params.LeftKeys;
			case UINavDirection::Right: return params.RightKeys;
			case UINavDirection::Up: return params.UpKeys;
			case UINavDirection::Down: break;
			}
			return params.DownKeys;
		}

		GamepadButton DPadOf(size_t direction)
		{
			switch (static_cast<UINavDirection>(direction))
			{
			case UINavDirection::Left: return GamepadButton::DPadLeft;
			case UINavDirection::Right: return GamepadButton::DPadRight;
			case UINavDirection::Up: return GamepadButton::DPadUp;
			case UINavDirection::Down: break;
			}
			return GamepadButton::DPadDown;
		}
	}

	int32_t FindNavTarget(const std::vector<UINavCandidate>& candidates, int32_t from, UINavDirection direction)
	{
		if (from < 0 || from >= static_cast<int32_t>(candidates.size()))
			return -1;

		const bool horizontal = direction == UINavDirection::Left || direction == UINavDirection::Right;
		const float sign = (direction == UINavDirection::Right || direction == UINavDirection::Down) ? 1.0f : -1.0f;
		const int main = horizontal ? 0 : 1;
		const int cross = 1 - main;

		const UIRect& origin = candidates[from].Rect;
		const glm::vec2 originCenter = CenterOf(origin);

		int32_t best = -1;
		float bestScore = 0.0f;
		for (int32_t i = 0; i < static_cast<int32_t>(candidates.size()); ++i)
		{
			if (i == from)
				continue;
			const UIRect& rect = candidates[i].Rect;
			const glm::vec2 center = CenterOf(rect);
			const float along = (center[main] - originCenter[main]) * sign;
			if (along <= k_MinStep)
				continue;

			const float gap = (std::max)(0.0f, (std::max)(rect.Min[cross] - origin.Max[cross], origin.Min[cross] - rect.Max[cross]));
			const float offset = std::abs(center[cross] - originCenter[cross]);
			const float score = along + 2.0f * gap + 0.25f * offset;
			if (best < 0 || score < bestScore)
			{
				best = i;
				bestScore = score;
			}
		}
		return best;
	}

	int32_t FindNearestNav(const std::vector<UINavCandidate>& candidates, const glm::vec2& point)
	{
		int32_t best = -1;
		float bestDistance = 0.0f;
		for (int32_t i = 0; i < static_cast<int32_t>(candidates.size()); ++i)
		{
			const glm::vec2 delta = CenterOf(candidates[i].Rect) - point;
			const float distance = delta.x * delta.x + delta.y * delta.y;
			if (best < 0 || distance < bestDistance)
			{
				best = i;
				bestDistance = distance;
			}
		}
		return best;
	}

	int32_t StepNavOrder(int32_t count, int32_t from, bool forward)
	{
		if (count <= 0)
			return -1;
		if (from < 0 || from >= count)
			return forward ? 0 : count - 1;
		return forward ? (from + 1) % count : (from + count - 1) % count;
	}

	bool UINavRepeater::Step(bool held, float deltaTime, float delay, float interval)
	{
		if (!held)
		{
			m_Held = false;
			return false;
		}
		if (!m_Held)
		{
			m_Held = true;
			m_Timer = delay;
			return true;
		}
		m_Timer -= deltaTime;
		if (m_Timer > 0.0f)
			return false;
		m_Timer = (std::max)(interval, 0.001f);
		return true;
	}

	uint32_t FindNavGamepad()
	{
		for (uint32_t pad = 0; pad < k_MaxGamepads; ++pad)
		{
			if (Input::IsGamepadConnected(pad))
				return pad;
		}
		return 0;
	}

	void ReadNavInput(UINavInput& out, bool keyboard, const UINavParams& params, uint32_t gamepad)
	{
		out = {};
		out.Gamepad = gamepad;

		if (keyboard)
		{
			for (size_t direction = 0; direction < k_UINavDirectionCount; ++direction)
				out.Direction[direction] = AnyDown(KeysOf(params, direction));
			out.Accept = AnyPressed(params.AcceptKeys);
			out.Back = AnyPressed(params.BackKeys);
			if (AnyPressed(params.NextKeys))
			{
				const bool shift = InputBackend::IsKeyDownUnconsumed(Key::LeftShift) || InputBackend::IsKeyDownUnconsumed(Key::RightShift);
				out.Next = !shift;
				out.Previous = shift;
			}
		}

		if (Input::IsGamepadConnected(gamepad))
		{
			for (size_t direction = 0; direction < k_UINavDirectionCount; ++direction)
				out.Direction[direction] = out.Direction[direction] || InputBackend::IsGamepadButtonDownUnconsumed(DPadOf(direction), gamepad);

			const glm::vec2 stick = Input::GetGamepadLeftStick(gamepad);
			const float threshold = params.StickThreshold;
			if (std::abs(stick.x) >= threshold || std::abs(stick.y) >= threshold)
			{
				if (std::abs(stick.x) >= std::abs(stick.y))
					out.Direction[static_cast<size_t>(stick.x < 0.0f ? UINavDirection::Left : UINavDirection::Right)] = true;
				else
					out.Direction[static_cast<size_t>(stick.y < 0.0f ? UINavDirection::Up : UINavDirection::Down)] = true;
			}

			out.Accept = out.Accept || InputBackend::IsGamepadButtonPressedUnconsumed(params.AcceptButton, gamepad);
			out.Back = out.Back || InputBackend::IsGamepadButtonPressedUnconsumed(params.BackButton, gamepad);
		}
	}

	void ConsumeNavInput(const UINavInput& fired, bool keyboard, const UINavParams& params)
	{
		for (size_t direction = 0; direction < k_UINavDirectionCount; ++direction)
		{
			if (!fired.Direction[direction])
				continue;
			if (keyboard)
				ConsumeKeys(KeysOf(params, direction));
			InputBackend::ConsumeGamepadButton(DPadOf(direction), fired.Gamepad);
		}
		if (fired.Accept)
		{
			if (keyboard)
				ConsumeKeys(params.AcceptKeys);
			InputBackend::ConsumeGamepadButton(params.AcceptButton, fired.Gamepad);
		}
		if (fired.Back)
		{
			if (keyboard)
				ConsumeKeys(params.BackKeys);
			InputBackend::ConsumeGamepadButton(params.BackButton, fired.Gamepad);
		}
		if (keyboard && (fired.Next || fired.Previous))
			ConsumeKeys(params.NextKeys);
	}

}
