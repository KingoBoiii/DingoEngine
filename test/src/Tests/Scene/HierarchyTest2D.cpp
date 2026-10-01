#include "HierarchyTest.h"

#include <DingoEngine/Physics/2D/Physics2D.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <format>

namespace
{
	using namespace Dingo;

	constexpr float k_Tolerance = 1e-4f;
	// Box2D builds and reads rotations through polynomial sin/cos/atan2 approximations, good to
	// about a tenth of a degree, so a body's angle never matches its transform more closely.
	constexpr float k_Box2DAngleTolerance = 0.25f;
	constexpr float k_ProbeHeight = 1.5f;
	const glm::vec4 k_MuzzleColor{ 1.0f, 0.1f, 0.9f, 1.0f };

	bool Near(const glm::vec3& a, const glm::vec3& b, float tolerance = k_Tolerance)
	{
		return glm::length(a - b) <= tolerance;
	}

	bool NearAngle(float a, float b, float tolerance = 1e-3f)
	{
		return std::abs(std::remainder(a - b, 360.0f)) <= tolerance;
	}

	bool Near(const glm::mat4& a, const glm::mat4& b, float tolerance = k_Tolerance)
	{
		for (int column = 0; column < 4; column++)
			for (int row = 0; row < 4; row++)
				if (std::abs(a[column][row] - b[column][row]) > tolerance)
					return false;
		return true;
	}

	Entity MakeEntity2D(Scene& scene, const std::string& name, const glm::vec3& position, float rotation, const glm::vec2& size = glm::vec2(1.0f))
	{
		Entity entity = scene.CreateEntity(name);
		TransformComponent& transform = entity.GetComponent<TransformComponent>();
		transform.Position = position;
		transform.Rotation = rotation;
		transform.Size = size;
		return entity;
	}

	// Written apart from the engine's version on purpose: the probe checks one against the other.
	void Apply(glm::vec2& position, float& rotation, const glm::vec2& localPosition, float localRotation)
	{
		const float angle = glm::radians(rotation);
		position += glm::vec2(std::cos(angle) * localPosition.x - std::sin(angle) * localPosition.y,
			std::sin(angle) * localPosition.x + std::cos(angle) * localPosition.y);
		rotation += localRotation;
	}

	float HullRotation(float t) { return 8.0f * std::sin(1.1f * t); }
	glm::vec2 HullPosition(float t) { return { 3.0f * std::sin(0.5f * t), -2.0f }; }
	float TurretRotation(float t) { return 40.0f * std::sin(0.7f * t); }

	constexpr glm::vec2 k_TurretOffset{ 0.0f, 0.9f };
	constexpr glm::vec2 k_BarrelOffset{ 1.3f, 0.05f };
	constexpr glm::vec2 k_MuzzleOffset{ 1.3f, 0.0f };
}

namespace Dingo
{

	void HierarchyTest::RunStructuralChecks2D()
	{
		Scene scene("Hierarchy Checks 2D");

		Entity a = MakeEntity2D(scene, "A", { 2.0f, 1.0f, 0.5f }, 30.0f, { 2.0f, 1.0f });
		Entity b = MakeEntity2D(scene, "B", { 1.0f, 0.0f, 0.25f }, 15.0f, { 0.5f, 0.5f });
		Entity c = MakeEntity2D(scene, "C", { 0.0f, 2.0f, 0.1f }, -60.0f, { 3.0f, 0.2f });
		b.SetParent(a, false);
		c.SetParent(b, false);

		{
			const TransformComponent& la = a.GetComponent<TransformComponent>();
			Check(a.GetWorldPosition2D() == la.Position && a.GetWorldRotation2D() == la.Rotation,
				"2D: a root's world pose is exactly its own TransformComponent");

			glm::vec2 position(2.0f, 1.0f);
			float rotation = 30.0f;
			Apply(position, rotation, { 1.0f, 0.0f }, 15.0f);
			Apply(position, rotation, { 0.0f, 2.0f }, -60.0f);
			Check(Near(c.GetWorldPosition2D(), { position.x, position.y, 0.85f }) && NearAngle(c.GetWorldRotation2D(), rotation),
				"2D: positions turn with the parent's rotation, and z and rotation add");

			Check(c.GetWorldTransform() == glm::mat4(1.0f) && c.GetWorldPosition() == glm::vec3(0.0f),
				"the unsuffixed world getters stay 3D: a 2D-only entity reads the identity");
		}

		{
			Entity d = MakeEntity2D(scene, "D", { -3.0f, 4.0f, 0.2f }, 75.0f);
			const glm::vec3 worldPosition = d.GetWorldPosition2D();
			const float worldRotation = d.GetWorldRotation2D();
			d.SetParent(c);
			const bool kept = Near(d.GetWorldPosition2D(), worldPosition) && NearAngle(d.GetWorldRotation2D(), worldRotation)
				&& !Near(d.GetComponent<TransformComponent>().Position, worldPosition);

			Entity e = MakeEntity2D(scene, "E", { 0.5f, -1.0f, 0.0f }, 10.0f);
			const TransformComponent before = e.GetComponent<TransformComponent>();
			e.SetParent(c, false);
			const TransformComponent& after = e.GetComponent<TransformComponent>();
			Check(kept && after.Position == before.Position && after.Rotation == before.Rotation,
				"2D: keepWorldTransform keeps the world pose; false keeps the local values");

			e.SetWorldPosition2D({ 7.0f, -2.0f, 0.4f });
			e.SetWorldRotation2D(123.0f);
			Check(Near(e.GetWorldPosition2D(), { 7.0f, -2.0f, 0.4f }) && NearAngle(e.GetWorldRotation2D(), 123.0f),
				"2D: SetWorldPosition2D and SetWorldRotation2D on a child land where asked");
		}

		{
			Entity parent3D = scene.CreateEntity("3D Parent");
			parent3D.AddComponent<Transform3DComponent>(Transform3DComponent({ 5.0f, 5.0f, 5.0f })).SetRotationEuler({ 10.0f, 20.0f, 30.0f });
			Entity sprite = MakeEntity2D(scene, "Sprite Under 3D", { 1.0f, 2.0f, 0.3f }, 45.0f);
			sprite.SetParent(parent3D, false);

			Entity parent2D = MakeEntity2D(scene, "2D Parent", { 3.0f, 4.0f, 0.0f }, 30.0f);
			Entity mesh = scene.CreateEntity("Mesh Under 2D");
			mesh.AddComponent<Transform3DComponent>(Transform3DComponent({ -1.0f, 0.5f, 2.0f }));
			mesh.SetParent(parent2D, false);

			Check(sprite.GetWorldPosition2D() == glm::vec3(1.0f, 2.0f, 0.3f) && sprite.GetWorldRotation2D() == 45.0f
				&& mesh.GetWorldPosition() == glm::vec3(-1.0f, 0.5f, 2.0f),
				"mixed trees: a 2D child of a 3D entity and a 3D child of a 2D entity see their parent as identity");
		}

		{
			Entity rig = MakeEntity2D(scene, "Camera Rig", { 4.0f, -3.0f, 0.0f }, 20.0f);
			Entity eye = MakeEntity2D(scene, "Camera Eye", { 2.0f, 1.0f, 0.0f }, 10.0f);
			eye.SetParent(rig, false);
			CameraComponent& camera = eye.AddComponent<CameraComponent>();
			camera.Type = CameraComponent::ProjectionType::Orthographic;

			const glm::vec3 position = eye.GetWorldPosition2D();
			const float aspect = 16.0f / 9.0f;
			const glm::mat4 expected = camera.GetProjection(aspect) * glm::inverse(
				glm::translate(glm::mat4(1.0f), glm::vec3(position.x, position.y, 0.0f))
				* glm::rotate(glm::mat4(1.0f), glm::radians(eye.GetWorldRotation2D()), glm::vec3(0.0f, 0.0f, 1.0f)));
			Check(Near(scene.GetCameraViewProjection(eye, aspect), expected) && !Near(position, { 2.0f, 1.0f, 0.0f }, 0.5f),
				"2D: an orthographic camera on a child views from its world pose");
		}
	}

	void HierarchyTest::BuildScene2D()
	{
		m_Font = Font::Create("assets/fonts/ArialBD.ttf");
		m_Scene2D = new Scene("Hierarchy Test 2D");

		Entity camera = m_Scene2D->CreateEntity("Camera");
		CameraComponent& cameraComponent = camera.AddComponent<CameraComponent>();
		cameraComponent.OrthographicSize = 18.0f;

		Entity ground = MakeEntity2D(*m_Scene2D, "Ground", { 0.0f, -7.5f, 0.0f }, 0.0f, { 22.0f, 1.0f });
		ground.AddComponent<SpriteRendererComponent>(SpriteRendererComponent({ 0.35f, 0.38f, 0.34f, 1.0f }));
		ground.AddComponent<RigidBody2DComponent>(RigidBody2DComponent(BodyType2D::Static));
		ground.AddComponent<BoxCollider2DComponent>();

		m_Hull2D = MakeEntity2D(*m_Scene2D, "Hull", { HullPosition(0.0f), 0.0f }, HullRotation(0.0f), { 4.0f, 1.6f });
		m_Hull2D.AddComponent<SpriteRendererComponent>(SpriteRendererComponent({ 0.35f, 0.5f, 0.3f, 1.0f }));
		m_Turret2D = MakeEntity2D(*m_Scene2D, "Turret", { k_TurretOffset, 0.1f }, TurretRotation(0.0f), { 1.8f, 1.0f });
		m_Turret2D.AddComponent<SpriteRendererComponent>(SpriteRendererComponent({ 0.42f, 0.58f, 0.36f, 1.0f }));
		m_Turret2D.SetParent(m_Hull2D, false);
		Entity barrel = MakeEntity2D(*m_Scene2D, "Barrel", { k_BarrelOffset, 0.05f }, 0.0f, { 2.4f, 0.3f });
		barrel.AddComponent<SpriteRendererComponent>(SpriteRendererComponent({ 0.22f, 0.26f, 0.22f, 1.0f }));
		barrel.SetParent(m_Turret2D, false);
		Entity muzzle = MakeEntity2D(*m_Scene2D, "Muzzle", { k_MuzzleOffset, 0.1f }, 0.0f, { 0.3f, 0.3f });
		muzzle.AddComponent<SpriteRendererComponent>(SpriteRendererComponent(k_MuzzleColor));
		muzzle.SetParent(barrel, false);
		for (float side : { -1.4f, 1.4f })
		{
			Entity wheel = MakeEntity2D(*m_Scene2D, "Wheel", { side, -0.85f, 0.2f }, 0.0f, { 0.7f, 0.7f });
			wheel.AddComponent<CircleRendererComponent>().Color = { 0.15f, 0.15f, 0.15f, 1.0f };
			wheel.SetParent(m_Hull2D, false);
		}
		if (m_Font)
		{
			Entity label = MakeEntity2D(*m_Scene2D, "Label", { -1.7f, 0.15f, 0.3f }, 0.0f);
			TextComponent& text = label.AddComponent<TextComponent>();
			text.Text = "HULL";
			text.Font = m_Font;
			text.Size = 0.4f;
			label.SetParent(m_Hull2D, false);
		}

		m_Carrier2D = MakeEntity2D(*m_Scene2D, "Carrier", { -7.0f, -6.5f, 0.0f }, 0.0f);
		Entity marker = MakeEntity2D(*m_Scene2D, "Carrier Marker", { 0.0f, 2.0f, 0.0f }, 0.0f, { 0.4f, 0.4f });
		marker.AddComponent<CircleRendererComponent>().Color = { 0.9f, 0.25f, 0.2f, 1.0f };
		marker.SetParent(m_Carrier2D, false);
		m_Crate2D = MakeEntity2D(*m_Scene2D, "Crate", glm::vec3(0.0f), 0.0f);
		m_Crate2D.AddComponent<SpriteRendererComponent>(SpriteRendererComponent({ 0.62f, 0.45f, 0.25f, 1.0f }));
		m_Crate2D.AddComponent<RigidBody2DComponent>(RigidBody2DComponent(BodyType2D::Dynamic));
		m_Crate2D.AddComponent<BoxCollider2DComponent>();
		m_Crate2D.SetParent(m_Carrier2D, false);

		m_Pivot2D = MakeEntity2D(*m_Scene2D, "Pivot", { 6.5f, 4.0f, 0.0f }, 0.0f);
		m_Paddle2D = MakeEntity2D(*m_Scene2D, "Paddle", { 2.0f, 0.0f, 0.0f }, 0.0f, { 0.4f, 1.4f });
		m_Paddle2D.AddComponent<SpriteRendererComponent>(SpriteRendererComponent({ 0.3f, 0.45f, 0.85f, 1.0f }));
		m_Paddle2D.AddComponent<RigidBody2DComponent>(RigidBody2DComponent(BodyType2D::Kinematic));
		m_Paddle2D.AddComponent<BoxCollider2DComponent>();
		m_Paddle2D.SetParent(m_Pivot2D, false);

		m_Platform2D = MakeEntity2D(*m_Scene2D, "Platform", { Platform2DPosition(0.0f), 0.0f }, 0.0f, { 2.4f, 0.4f });
		m_Platform2D.AddComponent<SpriteRendererComponent>(SpriteRendererComponent({ 0.55f, 0.5f, 0.35f, 1.0f }));
		m_Platform2D.AddComponent<RigidBody2DComponent>(RigidBody2DComponent(BodyType2D::Kinematic));
		m_Platform2D.AddComponent<BoxCollider2DComponent>();
		m_Rider2D = MakeEntity2D(*m_Scene2D, "Rider", { 0.5f, 0.6f, 0.1f }, 0.0f, { 0.6f, 0.6f });
		m_Rider2D.AddComponent<SpriteRendererComponent>(SpriteRendererComponent({ 0.9f, 0.6f, 0.2f, 1.0f }));
		m_Rider2D.AddComponent<RigidBody2DComponent>(RigidBody2DComponent(BodyType2D::Kinematic));
		m_Rider2D.AddComponent<BoxCollider2DComponent>();
		m_Rider2D.SetParent(m_Platform2D, false);

		m_Fallers2D.clear();
		for (const char* name : { "Faller C", "Faller B", "Faller A" })
		{
			Entity faller = MakeEntity2D(*m_Scene2D, name, { 1.3f, 0.0f, 0.0f }, 0.0f, { 0.8f, 0.8f });
			faller.AddComponent<SpriteRendererComponent>(SpriteRendererComponent({ 0.45f, 0.7f, 0.5f, 1.0f }));
			faller.AddComponent<RigidBody2DComponent>(RigidBody2DComponent(BodyType2D::Dynamic));
			faller.AddComponent<BoxCollider2DComponent>();
			m_Fallers2D.insert(m_Fallers2D.begin(), faller);
		}
		m_Fallers2D[0].GetComponent<TransformComponent>().Position = { -2.0f, 6.0f, 0.0f };
		m_Fallers2D[1].SetParent(m_Fallers2D[0], false);
		m_Fallers2D[2].SetParent(m_Fallers2D[1], false);

		Entity stand = MakeEntity2D(*m_Scene2D, "Stand", { 7.0f, -5.0f, 0.0f }, 30.0f);
		Entity post = MakeEntity2D(*m_Scene2D, "Post", { 1.0f, 0.0f, 0.0f }, 0.0f, { 0.5f, 1.5f });
		post.AddComponent<SpriteRendererComponent>(SpriteRendererComponent({ 0.7f, 0.7f, 0.75f, 1.0f }));
		post.AddComponent<RigidBody2DComponent>(RigidBody2DComponent(BodyType2D::Static));
		post.AddComponent<BoxCollider2DComponent>();
		post.SetParent(stand, false);

		m_Scene2D->OnStart();

		Physics2D* physics = m_Scene2D->GetPhysics2D();
		const PhysicsBodyId2D postBody = m_Scene2D->GetRuntimeBody2D(post);
		const glm::vec2 expectedPost(7.0f + std::cos(glm::radians(30.0f)), -5.0f + std::sin(glm::radians(30.0f)));
		const float postGap = physics ? glm::length(physics->GetPosition(postBody) - expectedPost) : 1.0f;
		const float postAngle = physics ? glm::degrees(physics->GetAngle(postBody)) : 0.0f;
		Check(postGap < 1e-4f && std::abs(postAngle - 30.0f) < k_Box2DAngleTolerance,
			std::format("2D: a body under a rotated parent is built at its world pose (gap {:.1e}, angle {:.3f} for 30)", postGap, postAngle));

		m_Scene2D->SetLinearVelocity(m_Fallers2D[0], glm::vec2(0.0f, 4.0f));
		m_Scene2D->SetLinearVelocity(m_Fallers2D[2], glm::vec2(0.0f, -2.0f));

		m_Crate2DStart = m_Crate2D.GetWorldPosition2D();
		m_Carrier2DStart = m_Carrier2D.GetWorldPosition2D();
		m_LastPlatform2DPosition = m_Platform2D.GetWorldPosition2D();
		m_LastPaddle2DPosition = m_Paddle2D.GetWorldPosition2D();
	}

	glm::vec2 HierarchyTest::Platform2DPosition(float time)
	{
		return { -7.0f + 1.2f * std::sin(1.6f * time), 2.5f + 1.2f * std::cos(1.6f * time) };
	}

	glm::vec2 HierarchyTest::ExpectedMuzzlePosition() const
	{
		glm::vec2 position = HullPosition(m_Time);
		float rotation = HullRotation(m_Time);
		Apply(position, rotation, k_TurretOffset, TurretRotation(m_Time));
		Apply(position, rotation, k_BarrelOffset, 0.0f);
		Apply(position, rotation, k_MuzzleOffset, 0.0f);
		return position;
	}

	void HierarchyTest::Animate2D(float deltaTime)
	{
		const float t = m_Time;
		TransformComponent& hull = m_Hull2D.GetComponent<TransformComponent>();
		hull.Position = { HullPosition(t), 0.0f };
		hull.Rotation = HullRotation(t);
		m_Turret2D.GetComponent<TransformComponent>().Rotation = TurretRotation(t);
		m_Carrier2D.GetComponent<TransformComponent>().Position.x = -7.0f + 2.0f * std::sin(1.3f * t);
		m_Pivot2D.GetComponent<TransformComponent>().Rotation = 50.0f * t;

		if (Physics2D* physics = m_Scene2D->GetPhysics2D())
			physics->MoveKinematic(m_Scene2D->GetRuntimeBody2D(m_Platform2D), Platform2DPosition(t), glm::radians(50.0f * t), deltaTime);
	}

	void HierarchyTest::TrackPhysics2D()
	{
		Physics2D* physics = m_Scene2D->GetPhysics2D();
		if (!physics)
			return;

		auto gap = [&](Entity entity)
		{
			return glm::length(glm::vec2(entity.GetWorldPosition2D()) - physics->GetPosition(m_Scene2D->GetRuntimeBody2D(entity)));
		};
		auto angleGap = [&](Entity entity)
		{
			return std::abs(std::remainder(entity.GetWorldRotation2D() - glm::degrees(physics->GetAngle(m_Scene2D->GetRuntimeBody2D(entity))), 360.0f));
		};

		m_MaxCrate2DGap = (std::max)(m_MaxCrate2DGap, gap(m_Crate2D));
		m_Carrier2DTravel = (std::max)(m_Carrier2DTravel, glm::length(m_Carrier2D.GetWorldPosition2D() - m_Carrier2DStart));
		m_MaxPaddle2DGap = (std::max)(m_MaxPaddle2DGap, gap(m_Paddle2D));
		const glm::vec3 paddle = m_Paddle2D.GetWorldPosition2D();
		m_Paddle2DTravel += glm::length(paddle - m_LastPaddle2DPosition);
		m_LastPaddle2DPosition = paddle;
		m_MaxPaddle2DAngleGap = (std::max)(m_MaxPaddle2DAngleGap, angleGap(m_Paddle2D));
		m_MaxRider2DGap = (std::max)(m_MaxRider2DGap, gap(m_Rider2D));
		m_MaxRider2DAngleGap = (std::max)(m_MaxRider2DAngleGap, angleGap(m_Rider2D));

		const glm::vec3 platform = m_Platform2D.GetWorldPosition2D();
		m_Platform2DTravel += glm::length(platform - m_LastPlatform2DPosition);
		m_LastPlatform2DPosition = platform;

		for (Entity faller : m_Fallers2D)
			m_MaxFaller2DGap = (std::max)(m_MaxFaller2DGap, gap(faller));
	}

	void HierarchyTest::RunPhysicsChecks2D()
	{
		const glm::vec3 crate = m_Crate2D.GetWorldPosition2D();
		const float drift = std::abs(crate.x - m_Crate2DStart.x);
		Check(m_MaxCrate2DGap < 1e-4f && drift < 0.05f && m_Carrier2DTravel > 1.0f,
			std::format("2D: a dynamic body under a moving parent stays where physics puts it (gap {:.1e}, drift {:.3f}, parent moved up to {:.2f})",
				m_MaxCrate2DGap, drift, m_Carrier2DTravel));
		Check(m_MaxPaddle2DGap < 1e-3f && m_MaxPaddle2DAngleGap < k_Box2DAngleTolerance && m_Paddle2DTravel > 1.0f,
			std::format("2D: a kinematic child follows its parent (gap {:.1e}, {:.3f} deg over {:.1f} units)", m_MaxPaddle2DGap, m_MaxPaddle2DAngleGap, m_Paddle2DTravel));
		Check(m_MaxRider2DGap < 1e-3f && m_MaxRider2DAngleGap < k_Box2DAngleTolerance && m_Platform2DTravel > 1.0f,
			std::format("2D: a kinematic child of a MoveKinematic'd parent keeps up within the step (gap {:.1e}, {:.3f} deg; the parent travelled {:.1f})",
				m_MaxRider2DGap, m_MaxRider2DAngleGap, m_Platform2DTravel));

		float lowest = 1e9f;
		for (Entity faller : m_Fallers2D)
			lowest = (std::min)(lowest, faller.GetWorldPosition2D().y);
		Check(m_MaxFaller2DGap < 1e-4f && m_Fallers2D.size() == 3 && lowest < -5.0f,
			std::format("2D: nested dynamic bodies are written back parents first (gap {:.1e} while falling)", m_MaxFaller2DGap));
	}

	void HierarchyTest::Render2D()
	{
		glm::mat4 viewProjection = m_Scene2D->GetActiveCameraViewProjection(m_AspectRatio);
		if (m_View == View::Probe2D)
		{
			const glm::vec2 muzzle = ExpectedMuzzlePosition();
			const float halfHeight = 0.5f * k_ProbeHeight;
			const float halfWidth = halfHeight * m_AspectRatio;
			viewProjection = glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, -1.0f, 1.0f)
				* glm::translate(glm::mat4(1.0f), glm::vec3(-muzzle, 0.0f));
		}

		m_Renderer2D->BeginScene(viewProjection);
		m_Renderer2D->Clear(m_ClearColor);
		m_Scene2D->RenderEntities(*m_Renderer2D);
		m_Renderer2D->EndScene();
	}

}
