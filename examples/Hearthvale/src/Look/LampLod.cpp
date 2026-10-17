#include "Look/LampLod.h"

#include <algorithm>

namespace Dingo
{

	int LampLod::CountInView(const std::vector<LampLodInput>& lights) const
	{
		int count = 0;
		for (size_t i = 0; i < lights.size() && i < m_Weight.size(); ++i)
		{
			if (lights[i].Overshoot <= 0.0f && m_Weight[i] > 0.0f)
				++count;
		}
		return count;
	}

	void LampLod::Update(float dt, const std::vector<LampLodInput>& lights, const glm::vec3& focus, int capacity)
	{
		const size_t count = lights.size();
		if (m_Weight.size() != count)
		{
			m_Weight.assign(count, 1.0f);
			m_Wanted.assign(count, 1);
			m_Key.assign(count, 0.0f);
			m_Primed = false;
		}
		capacity = std::max(0, capacity);

		m_Order.clear();
		for (size_t i = 0; i < count; ++i)
		{
			if (lights[i].Overshoot > m_ViewMargin)
			{
				m_Wanted[i] = 1;
				continue;
			}
			m_Key[i] = glm::distance(lights[i].Position, focus) - (m_Wanted[i] ? m_Stickiness : 0.0f);
			m_Order.push_back(i);
		}

		std::sort(m_Order.begin(), m_Order.end(), [this](size_t a, size_t b)
		{
			return m_Key[a] != m_Key[b] ? m_Key[a] < m_Key[b] : a < b;
		});
		for (size_t rank = 0; rank < m_Order.size(); ++rank)
			m_Wanted[m_Order[rank]] = static_cast<int>(rank) < capacity ? 1 : 0;

		if (!m_Primed)
		{
			for (size_t i = 0; i < count; ++i)
				m_Weight[i] = m_Wanted[i] ? 1.0f : 0.0f;
			m_Primed = true;
			return;
		}

		const float step = m_FadeSeconds > 0.0f ? dt / m_FadeSeconds : 1.0f;
		for (size_t i = 0; i < count; ++i)
		{
			const bool inZone = lights[i].Overshoot <= m_ViewMargin;
			if (!inZone || (m_Wanted[i] && m_Weight[i] > 0.0f))
				m_Weight[i] = std::min(1.0f, m_Weight[i] + step);
			else if (!m_Wanted[i])
				m_Weight[i] = std::max(0.0f, m_Weight[i] - step);
		}

		int planned = 0;
		for (size_t index : m_Order)
		{
			if (m_Weight[index] > 0.0f)
				++planned;
		}
		for (size_t index : m_Order)
		{
			if (planned >= capacity)
				break;
			if (m_Wanted[index] && m_Weight[index] <= 0.0f)
			{
				m_Weight[index] = std::min(1.0f, step);
				++planned;
			}
		}

		int exact = 0;
		for (size_t index : m_Order)
		{
			if (lights[index].Overshoot <= 0.0f && m_Weight[index] > 0.0f)
				++exact;
		}
		for (auto it = m_Order.rbegin(); exact > capacity && it != m_Order.rend(); ++it)
		{
			if (lights[*it].Overshoot <= 0.0f && m_Weight[*it] > 0.0f)
			{
				m_Weight[*it] = 0.0f;
				--exact;
			}
		}
	}

}
