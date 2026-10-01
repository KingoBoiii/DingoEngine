#pragma once
#include <DingoEngine.h>

namespace Dingo
{

	class KeepMap;

	class Hud
	{
	public:
		Hud(Scene& scene, const KeepMap& map, int startRoom);
		~Hud();

		Hud(const Hud&) = delete;
		Hud& operator=(const Hud&) = delete;

		// Corridors (-1) keep the last room's name.
		void SetRoom(int room);
		void Update();

	private:
		const KeepMap& m_Map;
		Font* m_Font = nullptr;
		Entity m_RoomLabel;
		int m_Room = -1;
	};

}
