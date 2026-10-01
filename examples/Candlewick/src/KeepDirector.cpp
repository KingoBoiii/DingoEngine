#include "KeepDirector.h"
#include "GameTuning.h"
#include "Overlay.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>

namespace
{
	using namespace Dingo;

	constexpr float k_RoomWidth = 10.0f;
	constexpr float k_RoomDepth = 8.0f;
	constexpr float k_CameraFocusHeight = 0.6f;
	constexpr const char* k_RoomName = "The Gatehouse";
}

namespace Dingo
{

	void KeepDirectorScript::OnStart()
	{
		Renderer3D& renderer3D = Application::Get().GetRenderer3D();
		m_BoxMesh = renderer3D.GetBoxMesh();
		m_SphereMesh = renderer3D.GetSphereMesh();

		m_BrassMaterial = renderer3D.CreateLitMaterial(MaterialParams()
			.SetDebugName("BrassLit")
			.SetRoughness(0.35f)
			.SetSpecular(0.5f));

		m_BrazierCoreMaterial = renderer3D.CreateLitMaterial(MaterialParams()
			.SetDebugName("BrazierCoreEmissive")
			.SetEmissiveColor(FLAME_COLOR)
			.SetEmissiveStrength(BRAZIER_EMISSIVE));

		m_SconceCoreMaterial = renderer3D.CreateLitMaterial(MaterialParams()
			.SetDebugName("SconceCoreEmissive")
			.SetEmissiveColor(FLAME_COLOR)
			.SetEmissiveStrength(SCONCE_EMISSIVE));

		SetupAmbient();
		BuildGatehouse();
		SetupCamera();
		SetupHud();
	}

	void KeepDirectorScript::OnUpdate(float)
	{
		if (Input::IsKeyPressed(Key::Escape))
		{
			RequestSceneTransition(SCENE_TITLE);
			return;
		}

		UpdateHud();
	}

	void KeepDirectorScript::OnDestroy()
	{
		DestroyAndDelete(m_BrassMaterial);
		DestroyAndDelete(m_BrazierCoreMaterial);
		DestroyAndDelete(m_SconceCoreMaterial);
		DestroyAndDelete(m_Font);
	}

	Entity KeepDirectorScript::SpawnSolid(const char* name, const glm::vec3& center, const glm::vec3& size, const glm::vec4& color, Material* material)
	{
		Entity entity = GetScene().CreateEntity(name);
		auto& transform = entity.AddComponent<Transform3DComponent>();
		transform.Position = center;
		transform.Scale = size;

		entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_BoxMesh, color)).Material = material;
		entity.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Static));
		entity.AddComponent<BoxCollider3DComponent>();
		return entity;
	}

	Entity KeepDirectorScript::SpawnGlow(const char* name, const glm::vec3& center, float diameter, Material* material)
	{
		Entity entity = GetScene().CreateEntity(name);
		auto& transform = entity.AddComponent<Transform3DComponent>();
		transform.Position = center;
		transform.Scale = glm::vec3(diameter);

		entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_SphereMesh, COLOR_EMBER)).Material = material;
		return entity;
	}

	Entity KeepDirectorScript::SpawnPointLight(const char* name, const glm::vec3& position, float intensity, float range)
	{
		Entity entity = GetScene().CreateEntity(name);
		entity.AddComponent<Transform3DComponent>().Position = position;
		entity.AddComponent<PointLightComponent>(PointLightComponent(FLAME_COLOR, intensity, range));
		return entity;
	}

	void KeepDirectorScript::SpawnWall(const glm::vec2& minCorner, const glm::vec2& maxCorner)
	{
		const glm::vec2 center = (minCorner + maxCorner) * 0.5f;
		const glm::vec2 extent = maxCorner - minCorner;
		SpawnSolid("Wall", { center.x, WALL_HEIGHT * 0.5f, center.y }, { extent.x, WALL_HEIGHT, extent.y }, COLOR_STONE);
	}

	void KeepDirectorScript::SpawnBrazier(const glm::vec3& floorPosition)
	{
		constexpr float standHeight = 0.8f;
		constexpr float bowlHeight = 0.15f;
		constexpr float coreDiameter = 0.5f;

		SpawnSolid("BrazierStand", floorPosition + glm::vec3(0.0f, standHeight * 0.5f, 0.0f), { 0.4f, standHeight, 0.4f }, COLOR_BRASS, m_BrassMaterial);
		SpawnSolid("BrazierBowl", floorPosition + glm::vec3(0.0f, standHeight + bowlHeight * 0.5f, 0.0f), { 0.7f, bowlHeight, 0.7f }, COLOR_BRASS, m_BrassMaterial);

		const glm::vec3 core = floorPosition + glm::vec3(0.0f, standHeight + bowlHeight + coreDiameter * 0.3f, 0.0f);
		SpawnGlow("BrazierCore", core, coreDiameter, m_BrazierCoreMaterial);
		SpawnPointLight("BrazierLight", core + glm::vec3(0.0f, BRAZIER_LIGHT_HEIGHT, 0.0f), BRAZIER_LIGHT_INTENSITY, BRAZIER_LIGHT_RANGE);
	}

	void KeepDirectorScript::SpawnSconce(const glm::vec3& wallPosition, const glm::vec3& inward)
	{
		constexpr float bracketSize = 0.2f;
		constexpr float coreDiameter = 0.24f;
		constexpr float lightOffset = 0.45f;

		const glm::vec3 base = wallPosition + glm::vec3(0.0f, SCONCE_HEIGHT, 0.0f);

		SpawnSolid("SconceBracket", base + inward * (bracketSize * 0.5f) - glm::vec3(0.0f, 0.3f, 0.0f), { bracketSize, 0.4f, bracketSize }, COLOR_BRASS, m_BrassMaterial);
		SpawnGlow("SconceCore", base + inward * (coreDiameter * 0.6f), coreDiameter, m_SconceCoreMaterial);
		SpawnPointLight("SconceLight", base + inward * lightOffset, SCONCE_LIGHT_INTENSITY, SCONCE_LIGHT_RANGE);
	}

	void KeepDirectorScript::SetupAmbient()
	{
		Entity ambient = GetScene().CreateEntity("Ambient");
		ambient.AddComponent<Transform3DComponent>();
		ambient.AddComponent<AmbientLightComponent>(AmbientLightComponent(AMBIENT_COLOR, AMBIENT_INTENSITY));
	}

	void KeepDirectorScript::BuildGatehouse()
	{
		const float halfW = k_RoomWidth * 0.5f;
		const float halfD = k_RoomDepth * 0.5f;
		const float wall = TILE_SIZE;

		SpawnSolid("Floor", { 0.0f, -FLOOR_THICKNESS * 0.5f, 0.0f }, { k_RoomWidth, FLOOR_THICKNESS, k_RoomDepth }, COLOR_FLOOR);

		SpawnWall({ -halfW, -halfD }, { halfW, -halfD + wall });
		SpawnWall({ -halfW, halfD - wall }, { halfW, halfD });
		SpawnWall({ -halfW, -halfD + wall }, { -halfW + wall, halfD - wall });
		SpawnWall({ halfW - wall, -halfD + wall }, { halfW, halfD - wall });

		SpawnBrazier({ 0.0f, 0.0f, 0.0f });

		const float northFace = -halfD + wall;
		const float westFace = -halfW + wall;
		const float eastFace = halfW - wall;
		SpawnSconce({ -2.5f, 0.0f, northFace }, { 0.0f, 0.0f, 1.0f });
		SpawnSconce({ 2.5f, 0.0f, northFace }, { 0.0f, 0.0f, 1.0f });
		SpawnSconce({ westFace, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f });
		SpawnSconce({ eastFace, 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f });
	}

	void KeepDirectorScript::SetupCamera()
	{
		Entity entity = GetScene().CreateEntity("Camera");
		auto& camera = entity.AddComponent<CameraComponent>();
		camera.Type = CameraComponent::ProjectionType::Perspective;
		camera.FOV = CAMERA_FOV;
		camera.PerspNear = CAMERA_NEAR;
		camera.PerspFar = CAMERA_FAR;
		camera.Primary = true;

		const float yaw = glm::radians(CAMERA_YAW_DEG);
		const float pitch = glm::radians(CAMERA_PITCH_DEG);
		const glm::vec3 offset = CAMERA_DISTANCE * glm::vec3(std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch));
		const glm::vec3 target(0.0f, k_CameraFocusHeight, 0.0f);

		auto& transform = entity.AddComponent<Transform3DComponent>();
		transform.Position = target + offset;
		transform.Rotation = glm::quat_cast(glm::inverse(glm::lookAt(transform.Position, target, glm::vec3(0.0f, 1.0f, 0.0f))));
	}

	void KeepDirectorScript::SetupHud()
	{
		m_Font = Overlay::LoadFont("HUD");

		Scene& scene = GetScene();
		Overlay::MakeCamera(scene, "UICamera", HUD_ORTHO_SIZE);

		m_RoomLabel = Overlay::MakeText(scene, m_Font, "RoomLabel", 0.55f, COLOR_TEXT, false);
		m_RoomLabel.GetComponent<TextComponent>().Text = k_RoomName;
		UpdateHud();
	}

	void KeepDirectorScript::UpdateHud()
	{
		const glm::vec2 viewport = Application::Get().GetRenderer2D().GetViewportSize();
		const float aspect = (viewport.y > 0.0f) ? viewport.x / viewport.y : 1.0f;
		const float halfH = HUD_ORTHO_SIZE * 0.5f;
		const float halfW = halfH * aspect;
		const float pad = 0.45f;

		m_RoomLabel.GetComponent<TransformComponent>().Position = { -halfW + pad, halfH - 0.95f, 0.0f };
	}

}
