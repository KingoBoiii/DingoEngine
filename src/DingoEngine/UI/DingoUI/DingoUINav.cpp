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

		bool IsDown(KeyCode key) { return InputBackend::IsKeyDownUnconsumed(key); }
		bool IsPressed(KeyCode key) { return InputBackend::IsKeyPressedUnconsumed(key); }

		uint32_t MaskOf(const std::vector<KeyCode>& keys, bool (*test)(KeyCode))
		{
			uint32_t mask = 0;
			for (size_t i = 0; i < keys.size() && i < 32; ++i)
			{
				if (test(keys[i]))
					mask |= 1u << i;
			}
			return mask;
		}

		void ConsumeMasked(const std::vector<KeyCode>& keys, uint32_t mask)
		{
			for (size_t i = 0; i < keys.size() && i < 32; ++i)
			{
				if (mask & (1u << i))
					InputBackend::ConsumeKey(keys[i]);
			}
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
			{
				out.DirectionKeys[direction] = MaskOf(KeysOf(params, direction), &IsDown);
				out.Direction[direction] = out.DirectionKeys[direction] != 0;
			}
			out.AcceptKeys = MaskOf(params.AcceptKeys, &IsPressed);
			out.BackKeys = MaskOf(params.BackKeys, &IsPressed);
			out.NextKeys = MaskOf(params.NextKeys, &IsPressed);
			out.Accept = out.AcceptKeys != 0;
			out.Back = out.BackKeys != 0;
			if (out.NextKeys != 0)
			{
				const bool shift = InputBackend::IsKeyDownUnconsumed(Key::LeftShift) || InputBackend::IsKeyDownUnconsumed(Key::RightShift);
				out.Next = !shift;
				out.Previous = shift;
			}
		}

		if (Input::IsGamepadConnected(gamepad))
		{
			for (size_t direction = 0; direction < k_UINavDirectionCount; ++direction)
			{
				out.DirectionPad[direction] = InputBackend::IsGamepadButtonDownUnconsumed(DPadOf(direction), gamepad);
				out.Direction[direction] = out.Direction[direction] || out.DirectionPad[direction];
			}

			const glm::vec2 stick = Input::GetGamepadLeftStick(gamepad);
			const float threshold = params.StickThreshold;
			if (std::abs(stick.x) >= threshold || std::abs(stick.y) >= threshold)
			{
				if (std::abs(stick.x) >= std::abs(stick.y))
					out.Direction[static_cast<size_t>(stick.x < 0.0f ? UINavDirection::Left : UINavDirection::Right)] = true;
				else
					out.Direction[static_cast<size_t>(stick.y < 0.0f ? UINavDirection::Up : UINavDirection::Down)] = true;
			}

			out.AcceptPad = InputBackend::IsGamepadButtonPressedUnconsumed(params.AcceptButton, gamepad);
			out.BackPad = InputBackend::IsGamepadButtonPressedUnconsumed(params.BackButton, gamepad);
			out.Accept = out.Accept || out.AcceptPad;
			out.Back = out.Back || out.BackPad;
		}
	}

	void ConsumeNavInput(const UINavInput& fired, bool keyboard, const UINavParams& params)
	{
		for (size_t direction = 0; direction < k_UINavDirectionCount; ++direction)
		{
			if (!fired.Direction[direction])
				continue;
			if (keyboard)
				ConsumeMasked(KeysOf(params, direction), fired.DirectionKeys[direction]);
			if (fired.DirectionPad[direction])
				InputBackend::ConsumeGamepadButton(DPadOf(direction), fired.Gamepad);
		}
		if (fired.Accept)
		{
			if (keyboard)
				ConsumeMasked(params.AcceptKeys, fired.AcceptKeys);
			if (fired.AcceptPad)
				InputBackend::ConsumeGamepadButton(params.AcceptButton, fired.Gamepad);
		}
		if (fired.Back)
		{
			if (keyboard)
				ConsumeMasked(params.BackKeys, fired.BackKeys);
			if (fired.BackPad)
				InputBackend::ConsumeGamepadButton(params.BackButton, fired.Gamepad);
		}
		if (keyboard && (fired.Next || fired.Previous))
			ConsumeMasked(params.NextKeys, fired.NextKeys);
	}

}
