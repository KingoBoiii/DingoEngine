#pragma once

#include <DingoEngine.h>

#include <vector>

namespace Dingo
{

	class Lantern
	{
	public:
		static constexpr glm::vec3 k_Position = { 2.5f, 1.3f, 2.0f };
		static constexpr float k_Scale = 1.5f;

		void Build(Scene& scene);
		void Release();
		void Update(float clock);
		void SetIntensityScale(float scale) { m_Scale = scale; }

		void SetEnabled(bool enabled);
		bool IsEnabled() const { return m_Enabled; }

		Entity GetSpot() const { return m_Spot; }
		Entity GetGlow() const { return m_Glow; }

	private:
		Entity m_Spot;
		Entity m_Glow;
		float m_SpotIntensity = 0.0f;
		float m_GlowIntensity = 0.0f;
		float m_Scale = 1.0f;
		bool m_Enabled = true;
		Mesh* m_Chain = nullptr;
	};

}
