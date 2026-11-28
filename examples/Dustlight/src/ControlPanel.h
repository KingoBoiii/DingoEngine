#pragma once

namespace Dingo
{

	class DustlightLayer;

	class ControlPanel
	{
	public:
		void SetVisible(bool visible) { m_Visible = visible; }
		bool IsVisible() const { return m_Visible; }
		void Update();
		void Draw(DustlightLayer& layer);

	private:
		bool m_Visible = true;
		int m_RaymarchResolution = 0;
		float m_RoomDensity = 0.1f;
		float m_SphereDensity = 0.08f;
		bool m_RoomEnabled = true;
		bool m_SphereEnabled = true;
	};

}
