#include "Look/BuildGhost.h"
#include "GameTuning.h"
#include "Look/ArtRegistry.h"
#include "World/WorldCells.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>

namespace Dingo
{

	namespace
	{
		float SurfaceAt(const TileGrid& grid, int x, int y)
		{
			if (!grid.InBounds(x, y))
				return GROUND_TOP;
			return (std::max)(GroundTop(GroundLookOf(grid.GetGround(x, y))), SOIL_TOP);
		}
	}

	BuildGhost::BuildGhost(Scene& scene)
		: m_Scene(scene)
	{
		m_Material = Application::Get().GetRenderer3D().CreateLitMaterial(MaterialParams()
			.SetDebugName("BuildGhost")
			.SetTranslucent(true));
	}

	BuildGhost::~BuildGhost()
	{
		Release();
		DestroyAndDelete(m_Material);
	}

	void BuildGhost::Release()
	{
		if (m_Marker)
			m_Scene.DestroyEntity(m_Marker);
		if (m_Ghost)
			m_Scene.DestroyEntity(m_Ghost);
		m_Marker = {};
		m_Ghost = {};
		m_GhostBuild = BuildId::Count;
	}

	void BuildGhost::SetMarker(const TileRect& area, const glm::vec4& color, const TileGrid& grid)
	{
		const float x = (static_cast<float>(area.X) + 0.5f * static_cast<float>(area.W)) * TILE_SIZE;
		const float z = (static_cast<float>(area.Y) + 0.5f * static_cast<float>(area.H)) * TILE_SIZE;
		const float surface = SurfaceAt(grid, area.X + area.W / 2, area.Y + area.H / 2);
		const glm::vec3 position(x, surface + TARGET_MARKER_LIFT + 0.5f * TARGET_MARKER_HEIGHT, z);

		if (!m_Marker)
		{
			m_Marker = Art::Spawn(m_Scene, Art::TARGET_TILE, "TargetMarker", position);
			if (!m_Marker)
				return;
			MeshRendererComponent& mesh = m_Marker.GetComponent<MeshRendererComponent>();
			mesh.Material = m_Material;
			mesh.Shadows = ShadowCasting::Off;
		}
		Transform3DComponent& transform = m_Marker.GetComponent<Transform3DComponent>();
		transform.Position = position;
		transform.Scale = glm::vec3(static_cast<float>(area.W) * TILE_SIZE, TARGET_MARKER_HEIGHT, static_cast<float>(area.H) * TILE_SIZE);
		m_Marker.GetComponent<MeshRendererComponent>().Color = color;
	}

	void BuildGhost::HideGhost()
	{
		if (m_Ghost)
			Art::ForEachMesh(m_Ghost, [](MeshRendererComponent& mesh) { mesh.Visible = false; });
	}

	void BuildGhost::Update(const BuildController& controller, const TileGrid& grid)
	{
		if (controller.GetMode() == BuildMode::Tools)
		{
			const TileCoord target = controller.GetAim().Target;
			SetMarker(TileRect{ target.X, target.Y, 1, 1 }, TARGET_COLOR_TOOL, grid);
			HideGhost();
			return;
		}

		const PlaceSpec spec = controller.GetSpec();
		const TileRect area = FootprintOf(spec);
		const bool valid = controller.GetPreview() == PlaceResult::Ok;
		SetMarker(area, valid ? TARGET_COLOR_VALID : TARGET_COLOR_INVALID, grid);

		if (m_Ghost && m_GhostBuild != spec.Id)
		{
			m_Scene.DestroyEntity(m_Ghost);
			m_Ghost = {};
		}
		const std::string_view artId = Art::ForBuild(spec.Id);
		const ArtEntry* art = Art::Find(artId);
		if (!art)
			return;

		const float x = (static_cast<float>(area.X) + 0.5f * static_cast<float>(area.W)) * TILE_SIZE;
		const float z = (static_cast<float>(area.Y) + 0.5f * static_cast<float>(area.H)) * TILE_SIZE;
		const float surface = SurfaceAt(grid, area.X + area.W / 2, area.Y + area.H / 2);
		const glm::vec3 position(x, surface + 0.5f * art->Size.y, z);

		if (!m_Ghost)
		{
			m_Ghost = Art::Spawn(m_Scene, artId, "BuildGhost", position);
			if (!m_Ghost)
				return;
			Art::ForEachMesh(m_Ghost, [this](MeshRendererComponent& mesh) {
				mesh.Material = m_Material;
				mesh.Shadows = ShadowCasting::Off;
			});
			m_GhostBuild = spec.Id;
		}
		Transform3DComponent& transform = m_Ghost.GetComponent<Transform3DComponent>();
		transform.Position = position;
		transform.Rotation = glm::angleAxis(glm::half_pi<float>() * static_cast<float>(spec.Rotation & 3), glm::vec3(0.0f, 1.0f, 0.0f));
		const glm::vec4 color = valid ? GHOST_COLOR_VALID : GHOST_COLOR_INVALID;
		Art::ForEachMesh(m_Ghost, [color](MeshRendererComponent& mesh) {
			mesh.Visible = true;
			mesh.Color = color;
		});
	}

}
