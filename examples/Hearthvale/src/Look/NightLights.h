#pragma once
#include <DingoEngine.h>

#include "Look/LampLod.h"
#include "Look/Vfx.h"
#include "World/TileGrid.h"

#include <cstdint>
#include <map>
#include <vector>

namespace Dingo
{

	// What glows at night: a point light and bulb on every lamp post and festival lantern, a porch light and two
	// glowing windows on every house, and a chimney (and festival sparks) that smoke. The lights go through LampLod so
	// the ones in view stay under the renderer's budget. Sites follow the grid's occupants: Rescan after the world
	// rebuilt cells. Windows and bulbs are not Static, so changing their glow never rebakes a cell. Device side:
	// never constructed headless.
	class NightLights
	{
	public:
		NightLights(Scene& scene, const TileGrid& grid, Vfx& vfx);
		~NightLights();
		NightLights(const NightLights&) = delete;
		NightLights& operator=(const NightLights&) = delete;

		void Rescan();

		void Update(float dt, float lampAmount, float chimneyAmount, const glm::mat4& viewProjection, const glm::vec3& focus);

		int GetLightCount() const { return static_cast<int>(m_Sites.size()); }
		int GetLitCount() const { return m_Lit; }

	private:
		enum class Kind : uint8_t { Lamp, Lantern, House };

		struct Site
		{
			Kind What = Kind::Lamp;
			Occupant Source;
			glm::vec3 Position{ 0.0f };
			glm::vec3 Color{ 1.0f };
			float Intensity = 1.0f;
			float Range = 1.0f;
			Entity Light;
			Entity Bulb;
			Entity Windows[2];
			Entity Chimney;
			Entity Sparks;
			float BulbGlow = -1.0f;
			float WindowGlow[2] = { -1.0f, -1.0f };
		};

		void Create(const Occupant& occupant);
		void Destroy(Site& site);
		void WriteGlow(Entity entity, const glm::vec3& color, float strength, float roughness, float specular, float& written) const;

	private:
		Scene& m_Scene;
		const TileGrid& m_Grid;
		Vfx& m_Vfx;
		std::map<uint32_t, Site> m_Sites;
		std::vector<Site*> m_Order;
		std::vector<LampLodInput> m_Inputs;
		LampLod m_Lod;
		bool m_Dirty = true;
		int m_Lit = 0;
	};

}
