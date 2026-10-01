#pragma once
#include "KeepMap.h"

#include <DingoEngine.h>

#include <glm/glm.hpp>

#include <optional>

namespace Dingo
{

	// Three-quarter camera at a fixed yaw. It follows the player with a lag, or, given a room
	// rectangle, hangs still over that room at the distance that fits it.
	class CameraRig
	{
	public:
		CameraRig(Scene& scene, const glm::vec3& focus, const std::optional<TileRect>& overviewRoom);

		void Update(float deltaTime, const glm::vec3& playerFeet);

		const glm::vec3& GetEye() const { return m_Eye; }

	private:
		void Apply(const glm::vec3& focus, float distance);

	private:
		Entity m_Entity;
		std::optional<TileRect> m_OverviewRoom;

		glm::vec3 m_Focus{ 0.0f };
		glm::vec3 m_Eye{ 0.0f };
		float m_FittedAspect = 0.0f;
		float m_FittedDistance = 0.0f;
	};

}
