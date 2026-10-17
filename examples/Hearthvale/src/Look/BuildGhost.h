#pragma once
#include <DingoEngine.h>

#include "Placing/BuildController.h"
#include "Defs/BuildDefs.h"

#include <string_view>

namespace Dingo
{

	// The target marker (one tile, or the footprint in build mode, tinted valid or invalid) and the buildable's
	// ghost, drawn translucent through the art registry. GPU side: never constructed headless.
	class BuildGhost
	{
	public:
		explicit BuildGhost(Scene& scene);
		~BuildGhost();
		BuildGhost(const BuildGhost&) = delete;
		BuildGhost& operator=(const BuildGhost&) = delete;

		// Once a frame, after the controller stepped.
		void Update(const BuildController& controller, const TileGrid& grid);

		// Destroys the entities. The material goes in the destructor, after the scene has been cleared.
		void Release();

	private:
		void SetMarker(const TileRect& area, const glm::vec4& color, const TileGrid& grid);
		void HideGhost();

	private:
		Scene& m_Scene;
		Material* m_Material = nullptr;
		Entity m_Marker;
		Entity m_Ghost;
		BuildId m_GhostBuild = BuildId::Count;
	};

}
