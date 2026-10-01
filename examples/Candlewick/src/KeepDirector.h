#pragma once
#include <DingoEngine.h>

#include <glm/glm.hpp>

namespace Dingo
{

	class KeepDirectorScript : public ScriptableEntity
	{
	protected:
		void OnStart() override;
		void OnUpdate(float deltaTime) override;
		void OnDestroy() override;

	private:
		void BuildGatehouse();
		void SetupAmbient();
		void SetupCamera();
		void SetupHud();
		void UpdateHud();

		Entity SpawnSolid(const char* name, const glm::vec3& center, const glm::vec3& size, const glm::vec4& color, Material* material = nullptr);
		Entity SpawnGlow(const char* name, const glm::vec3& center, float diameter, Material* material);
		Entity SpawnPointLight(const char* name, const glm::vec3& position, float intensity, float range);

		void SpawnWall(const glm::vec2& minCorner, const glm::vec2& maxCorner);
		void SpawnBrazier(const glm::vec3& floorPosition);
		// wallPosition is the point on the wall's inner face at floor level; inward is the unit axis pointing into the room.
		void SpawnSconce(const glm::vec3& wallPosition, const glm::vec3& inward);

	private:
		Mesh* m_BoxMesh = nullptr;
		Mesh* m_SphereMesh = nullptr;

		Material* m_BrassMaterial = nullptr;
		Material* m_BrazierCoreMaterial = nullptr;
		Material* m_SconceCoreMaterial = nullptr;

		Font* m_Font = nullptr;
		Entity m_RoomLabel;
	};

}
