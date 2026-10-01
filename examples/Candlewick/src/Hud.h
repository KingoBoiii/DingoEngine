#pragma once
#include <DingoEngine.h>

namespace Dingo
{

	class KeepMap;
	class Lantern;

	class Hud
	{
	public:
		Hud(Scene& scene, const KeepMap& map, const Lantern& lantern, int startRoom);
		~Hud();

		Hud(const Hud&) = delete;
		Hud& operator=(const Hud&) = delete;

		// Corridors (-1) keep the last room's name.
		void SetRoom(int room);
		void Update(const Lantern& lantern);

	private:
		const KeepMap& m_Map;
		Font* m_Font = nullptr;
		Entity m_RoomLabel;
		Entity m_OilBackground;
		Entity m_OilFill;
		Entity m_LanternState;
		Entity m_KeyHint;
		int m_Room = -1;
	};

}
