#include "Look/NightLights.h"
#include "GameTuning.h"
#include "Look/ArtRegistry.h"
#include "Look/NightCurve.h"
#include "World/ValleyLayout.h"
#include "World/WorldCells.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace Dingo
{

	namespace
	{
		bool HasHouseLights(Landmark landmark)
		{
			return landmark != Landmark::ShippingBin && landmark != Landmark::NoticeBoard;
		}

		float GroundAt(const TileGrid& grid, const Occupant& occupant)
		{
			return GroundTop(GroundLookOf(grid.GetGround(occupant.X + occupant.W / 2, occupant.Y + occupant.H / 2)));
		}

		bool Wants(const Occupant& occupant)
		{
			switch (occupant.Kind)
			{
				case OccupantKind::Building:
				{
					const BuildId id = static_cast<BuildId>(occupant.DefId);
					return id == BuildId::LampPost || id == BuildId::FestivalLantern;
				}
				case OccupantKind::Landmark: return HasHouseLights(static_cast<Landmark>(occupant.DefId));
				case OccupantKind::Decoration: break;
			}
			return false;
		}

		Entity SpawnPart(Scene& scene, std::string_view id, const char* name, const glm::vec3& position)
		{
			Entity entity = Art::Spawn(scene, id, name, position);
			if (entity.IsValid())
				entity.GetComponent<MeshRendererComponent>().Shadows = ShadowCasting::Off;
			return entity;
		}
	}

	NightLights::NightLights(Scene& scene, const TileGrid& grid, Vfx& vfx)
		: m_Scene(scene), m_Grid(grid), m_Vfx(vfx), m_Lod(LIGHT_LOD_FADE_TIME, LIGHT_LOD_VIEW_MARGIN, LIGHT_LOD_STICKINESS)
	{}

	NightLights::~NightLights()
	{
		for (auto& [id, site] : m_Sites)
			Destroy(site);
	}

	void NightLights::Destroy(Site& site)
	{
		for (Entity* entity : { &site.Light, &site.Bulb, &site.Windows[0], &site.Windows[1], &site.Chimney, &site.Sparks })
		{
			if (entity->IsValid())
				m_Scene.DestroyEntity(*entity);
			*entity = {};
		}
	}

	void NightLights::Rescan()
	{
		std::set<uint32_t> present;
		for (const Occupant& occupant : m_Grid.GetOccupants())
		{
			if (!Wants(occupant))
				continue;
			present.insert(occupant.Id);
			if (m_Sites.find(occupant.Id) == m_Sites.end())
				Create(occupant);
		}
		for (auto it = m_Sites.begin(); it != m_Sites.end();)
		{
			if (present.count(it->first) == 0)
			{
				Destroy(it->second);
				it = m_Sites.erase(it);
			}
			else
			{
				++it;
			}
		}

		m_Order.clear();
		m_Inputs.clear();
		for (auto& [id, site] : m_Sites)
		{
			m_Order.push_back(&site);
			m_Inputs.push_back(LampLodInput{ site.Position, 0.0f });
		}
	}

	void NightLights::Create(const Occupant& occupant)
	{
		Site site;
		const float ground = GroundAt(m_Grid, occupant);
		const float cx = (static_cast<float>(occupant.X) + 0.5f * static_cast<float>(occupant.W)) * TILE_SIZE;
		const float cz = (static_cast<float>(occupant.Y) + 0.5f * static_cast<float>(occupant.H)) * TILE_SIZE;

		if (occupant.Kind == OccupantKind::Building)
		{
			const bool lamp = static_cast<BuildId>(occupant.DefId) == BuildId::LampPost;
			site.What = lamp ? Kind::Lamp : Kind::Lantern;
			site.Color = lamp ? LAMP_COLOR : LANTERN_COLOR;
			site.Intensity = lamp ? LAMP_INTENSITY : LANTERN_INTENSITY;
			site.Range = lamp ? LAMP_RANGE : LANTERN_RANGE;
			site.Position = glm::vec3(cx, ground + (lamp ? LAMP_HEIGHT : LANTERN_HEIGHT), cz);
			site.Bulb = SpawnPart(m_Scene, Art::LAMP_BULB, "LampBulb", site.Position);
			if (!lamp)
				site.Sparks = m_Vfx.SpawnSparks(site.Position + glm::vec3(0.0f, 0.15f, 0.0f));
		}
		else
		{
			const Landmark landmark = static_cast<Landmark>(occupant.DefId);
			const LandmarkSpec& spec = GetLandmarkSpec(landmark);
			const ArtEntry* art = Art::Find(Art::ForLandmark(landmark));
			const glm::vec3 size = art ? art->Size : glm::vec3(3.6f, 3.0f, 3.6f);
			const float away = static_cast<float>(spec.DoorY) + 0.5f >= cz ? 1.0f : -1.0f;

			site.What = Kind::House;
			site.Color = HOUSE_LIGHT_COLOR;
			site.Intensity = HOUSE_LIGHT_INTENSITY;
			site.Range = HOUSE_LIGHT_RANGE;
			site.Position = glm::vec3((static_cast<float>(spec.DoorX) + 0.5f) * TILE_SIZE, ground + HOUSE_LIGHT_HEIGHT,
				(static_cast<float>(spec.DoorY) + 0.5f) * TILE_SIZE + away * HOUSE_LIGHT_FORWARD);

			const float faceZ = cz + away * (0.5f * size.z + 0.03f);
			const float windowY = ground + (std::min)(1.4f, 0.55f * size.y);
			site.Windows[0] = SpawnPart(m_Scene, Art::WINDOW, "Window", glm::vec3(cx - 1.1f, windowY, faceZ));
			site.Windows[1] = SpawnPart(m_Scene, Art::WINDOW, "Window", glm::vec3(cx + 1.1f, windowY, faceZ));
			site.Chimney = m_Vfx.SpawnChimney(glm::vec3(cx + 0.25f * size.x, ground + size.y + 0.1f, cz));
		}

		site.Light = m_Scene.CreateEntity("NightLight");
		site.Light.AddComponent<Transform3DComponent>().Position = site.Position;
		PointLightComponent& light = site.Light.AddComponent<PointLightComponent>(PointLightComponent(site.Color, site.Intensity, site.Range));
		light.Enabled = false;
		m_Sites.emplace(occupant.Id, std::move(site));
	}

	void NightLights::WriteGlow(Entity entity, const glm::vec3& color, float strength, float roughness, float specular, float& written) const
	{
		if (!entity.IsValid())
			return;
		const bool settled = strength <= 0.0f ? written <= 0.0f : std::abs(strength - written) < GLOW_WRITE_EPSILON;
		if (settled && written >= 0.0f)
			return;
		written = strength;
		entity.GetComponent<MeshRendererComponent>().SetSurface(
			SurfaceOverride().SetEmissiveColor(color).SetEmissiveStrength(strength).SetRoughness(roughness).SetSpecular(specular));
	}

	void NightLights::Update(float dt, float lampAmount, float chimneyAmount, const glm::mat4& viewProjection, const glm::vec3& focus)
	{
		m_Lit = 0;
		const bool on = lampAmount > 0.0f;

		if (on && !m_Order.empty())
		{
			const Frustum frustum = Frustum::FromViewProjection(viewProjection);
			for (size_t i = 0; i < m_Order.size(); ++i)
				m_Inputs[i].Overshoot = frustum.DistanceOutside(m_Order[i]->Position, m_Order[i]->Range);
			const int budget = static_cast<int>(Application::Get().GetRenderer3D().GetLocalLightBudget());
			m_Lod.Update(dt, m_Inputs, focus, (std::max)(0, budget - LIGHT_LOD_HEADROOM));
		}

		for (size_t i = 0; i < m_Order.size(); ++i)
		{
			Site& site = *m_Order[i];
			const float weight = on && i < m_Lod.GetWeights().size() ? m_Lod.GetWeights()[i] : 0.0f;
			const float level = lampAmount * weight;

			PointLightComponent& light = site.Light.GetComponent<PointLightComponent>();
			light.Enabled = level > 0.001f;
			light.Intensity = site.Intensity * level;
			if (light.Enabled)
				++m_Lit;

			WriteGlow(site.Bulb, BULB_GLOW_COLOR, BULB_GLOW_STRENGTH * lampAmount, 0.3f, 0.0f, site.BulbGlow);
			for (size_t window = 0; window < 2; ++window)
				WriteGlow(site.Windows[window], WINDOW_GLOW_COLOR, WINDOW_GLOW_STRENGTH * lampAmount, 0.2f, 0.3f, site.WindowGlow[window]);

			if (site.Chimney.IsValid())
				site.Chimney.GetComponent<ParticleEmitterComponent>().RateScale = chimneyAmount;
			if (site.Sparks.IsValid())
				site.Sparks.GetComponent<ParticleEmitterComponent>().RateScale = lampAmount;
		}
	}

}
