#include "LightLod.h"
#include "GameTuning.h"

#include <algorithm>

namespace
{
	using namespace Dingo;

	struct Load
	{
		int Exact = 0;
		int Planned = 0;
	};

	Load CountGameplayLights(const std::vector<Entity>& lights, const Frustum& frustum)
	{
		Load load;
		for (Entity light : lights)
		{
			if (!light.IsValid())
				continue;

			bool enabled = false;
			float range = 0.0f;
			float intensity = 0.0f;
			if (light.HasComponent<PointLightComponent>())
			{
				const auto& point = light.GetComponent<PointLightComponent>();
				enabled = point.Enabled;
				range = point.Range;
				intensity = point.Intensity;
			}
			else if (light.HasComponent<SpotLightComponent>())
			{
				const auto& spot = light.GetComponent<SpotLightComponent>();
				enabled = spot.Enabled;
				range = spot.Range;
				intensity = spot.Intensity;
			}

			if (!enabled || range <= 0.0f || intensity <= 0.0f)
				continue;

			const float overshoot = frustum.DistanceOutside(light.GetComponent<Transform3DComponent>().Position, range);
			if (overshoot <= 0.0f)
				++load.Exact;
			if (overshoot <= LIGHT_LOD_VIEW_MARGIN)
				++load.Planned;
		}
		return load;
	}
}

namespace Dingo
{

	LightLod::LightLod(const std::vector<DecorFlame>& flames, bool enabled)
		: m_Enabled(enabled)
	{
		m_Flames.reserve(flames.size());
		for (const DecorFlame& flame : flames)
		{
			Flame entry;
			entry.Light = flame.Light;
			entry.BaseIntensity = flame.BaseIntensity;
			m_Flames.push_back(entry);
		}
		m_Order.reserve(m_Flames.size());
	}

	void LightLod::AddGameplayLight(Entity light)
	{
		m_Gameplay.push_back(light);
	}

	void LightLod::SetFlicker(size_t flame, float factor)
	{
		if (flame < m_Flames.size())
			m_Flames[flame].Flicker = factor;
	}

	void LightLod::Write(Flame& flame)
	{
		auto& light = flame.Light.GetComponent<PointLightComponent>();
		light.Enabled = flame.Weight > 0.0f;
		light.Intensity = flame.BaseIntensity * flame.Weight * flame.Flicker;
	}

	void LightLod::Update(float deltaTime, const glm::vec3& focus, const glm::mat4& viewProjection)
	{
		if (!m_Enabled)
		{
			for (Flame& flame : m_Flames)
			{
				if (flame.Light.IsValid())
					Write(flame);
			}
			return;
		}

		const int budget = static_cast<int>(Application::Get().GetRenderer3D().GetLocalLightBudget());
		const int capacity = std::max(0, budget - LIGHT_LOD_HEADROOM);
		// The frustum and sphere test Renderer3D culls lights with, so what this counts is what it keeps.
		const Frustum frustum = Frustum::FromViewProjection(viewProjection);
		const Load gameplay = CountGameplayLights(m_Gameplay, frustum);
		const int slots = std::max(0, capacity - gameplay.Planned);

		if (!m_OverCapacityWarned && gameplay.Exact > capacity)
		{
			m_OverCapacityWarned = true;
			DE_WARN("Candlewick: {} gameplay lights are in view but the light LOD's capacity is {} (budget {} less {} headroom); "
				"the renderer may drop a warden's eye, so the cone drawn and the cone tested can differ", gameplay.Exact, capacity, budget, LIGHT_LOD_HEADROOM);
		}

		m_Order.clear();
		for (size_t i = 0; i < m_Flames.size(); ++i)
		{
			Flame& flame = m_Flames[i];
			if (!flame.Light.IsValid())
				continue;

			const glm::vec3 position = flame.Light.GetComponent<Transform3DComponent>().Position;
			flame.Overshoot = frustum.DistanceOutside(position, flame.Light.GetComponent<PointLightComponent>().Range);
			if (flame.Overshoot > LIGHT_LOD_VIEW_MARGIN)
			{
				flame.Wanted = true;
				continue;
			}

			flame.Key = glm::distance(position, focus) - (flame.Wanted ? LIGHT_LOD_STICKINESS : 0.0f);
			m_Order.push_back(i);
		}

		std::sort(m_Order.begin(), m_Order.end(), [this](size_t a, size_t b)
		{
			const float keyA = m_Flames[a].Key;
			const float keyB = m_Flames[b].Key;
			return keyA != keyB ? keyA < keyB : a < b;
		});
		for (size_t rank = 0; rank < m_Order.size(); ++rank)
			m_Flames[m_Order[rank]].Wanted = static_cast<int>(rank) < slots;

		if (!m_Primed)
		{
			for (Flame& flame : m_Flames)
			{
				if (!flame.Light.IsValid())
					continue;
				flame.Weight = flame.Wanted ? 1.0f : 0.0f;
				Write(flame);
			}

			DE_INFO("Candlewick: light LOD budget {}, capacity {} ({} headroom), {} gameplay lights near the view, {} slots for {} flames",
				budget, capacity, LIGHT_LOD_HEADROOM, gameplay.Planned, slots, m_Flames.size());
			m_Primed = true;
			return;
		}

		const float step = deltaTime / LIGHT_LOD_FADE_TIME;
		for (Flame& flame : m_Flames)
		{
			if (!flame.Light.IsValid())
				continue;

			const bool inZone = flame.Overshoot <= LIGHT_LOD_VIEW_MARGIN;
			if (!inZone || (flame.Wanted && flame.Weight > 0.0f))
				flame.Weight = std::min(1.0f, flame.Weight + step);
			else if (!flame.Wanted)
				flame.Weight = std::max(0.0f, flame.Weight - step);
		}

		int planned = gameplay.Planned;
		for (size_t index : m_Order)
		{
			if (m_Flames[index].Weight > 0.0f)
				++planned;
		}
		for (size_t index : m_Order)
		{
			if (planned >= capacity)
				break;

			Flame& flame = m_Flames[index];
			if (flame.Wanted && flame.Weight <= 0.0f)
			{
				flame.Weight = std::min(1.0f, step);
				++planned;
			}
		}

		int exact = gameplay.Exact;
		for (size_t index : m_Order)
		{
			if (m_Flames[index].Overshoot <= 0.0f && m_Flames[index].Weight > 0.0f)
				++exact;
		}
		for (auto it = m_Order.rbegin(); exact > capacity && it != m_Order.rend(); ++it)
		{
			Flame& flame = m_Flames[*it];
			if (flame.Overshoot <= 0.0f && flame.Weight > 0.0f)
			{
				flame.Weight = 0.0f;
				--exact;
			}
		}

		for (Flame& flame : m_Flames)
		{
			if (flame.Light.IsValid())
				Write(flame);
		}
	}

}
