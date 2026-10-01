#include "Hud.h"
#include "GameTuning.h"
#include "KeepMap.h"
#include "Overlay.h"

namespace Dingo
{

	Hud::Hud(Scene& scene, const KeepMap& map, int startRoom)
		: m_Map(map)
	{
		m_Font = Overlay::LoadFont("HUD");
		Overlay::MakeCamera(scene, "UICamera", HUD_ORTHO_SIZE);
		m_RoomLabel = Overlay::MakeText(scene, m_Font, "RoomLabel", HUD_ROOM_LABEL_SIZE, COLOR_TEXT, false);

		SetRoom(startRoom);
		Update();
	}

	Hud::~Hud()
	{
		DestroyAndDelete(m_Font);
	}

	void Hud::SetRoom(int room)
	{
		if (room < 0 || room == m_Room || room >= static_cast<int>(m_Map.GetRooms().size()))
			return;

		m_Room = room;
		m_RoomLabel.GetComponent<TextComponent>().Text = m_Map.GetRooms()[room].Name;
	}

	void Hud::Update()
	{
		const glm::vec2 viewport = Application::Get().GetRenderer2D().GetViewportSize();
		const float aspect = (viewport.y > 0.0f) ? viewport.x / viewport.y : 1.0f;
		const float halfH = HUD_ORTHO_SIZE * 0.5f;
		const float halfW = halfH * aspect;

		m_RoomLabel.GetComponent<TransformComponent>().Position = { -halfW + HUD_PADDING, halfH - HUD_ROOM_LABEL_DROP, 0.0f };
	}

}
