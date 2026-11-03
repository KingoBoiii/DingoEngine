#include "FixedTickTest.h"

#include <imgui.h>

#include <cmath>
#include <cstdint>
#include <format>
#include <vector>

namespace
{
	using namespace Dingo;

	constexpr float k_TickRates[] = { 10.0f, 20.0f, 60.0f };
	constexpr const char* k_TickRateNames[] = { "10 Hz", "20 Hz", "60 Hz" };
	constexpr uint32_t k_RecordTick = 590;

	struct Probe
	{
		uint32_t Calls = 0;
		bool Recorded = false;
		glm::vec3 Ball{ 0.0f };
		glm::quat BallRotation{ 1.0f, 0.0f, 0.0f, 0.0f };
		glm::vec3 Roller{ 0.0f };
		glm::vec3 Box2D{ 0.0f };
		float Box2DRotation = 0.0f;

		bool operator==(const Probe& other) const
		{
			return Recorded && other.Recorded && Ball == other.Ball && BallRotation == other.BallRotation
				&& Roller == other.Roller && Box2D == other.Box2D && Box2DRotation == other.Box2DRotation;
		}
	};

	// Pushes the ball in every fixed step and, at the start of the step after k_RecordTick, records
	// the world as k_RecordTick steps left it.
	class PushScript : public ScriptableEntity
	{
	public:
		PushScript(Probe* probe, Entity roller, Entity box2D) : m_Probe(probe), m_Roller(roller), m_Box2D(box2D) {}

		void OnFixedUpdate(float step) override
		{
			if (++m_Probe->Calls == k_RecordTick + 1)
			{
				const Transform3DComponent& ball = GetEntity().GetComponent<Transform3DComponent>();
				m_Probe->Ball = ball.Position;
				m_Probe->BallRotation = ball.Rotation;
				m_Probe->Roller = m_Roller.GetComponent<Transform3DComponent>().Position;
				m_Probe->Box2D = m_Box2D.GetComponent<TransformComponent>().Position;
				m_Probe->Box2DRotation = m_Box2D.GetComponent<TransformComponent>().Rotation;
				m_Probe->Recorded = true;
			}
			GetScene().ApplyForce(GetEntity(), glm::vec3(0.0f, 0.0f, 3.0f));
		}

	private:
		Probe* m_Probe;
		Entity m_Roller;
		Entity m_Box2D;
	};

	struct World
	{
		Entity Ball;
		Entity Roller;
		Entity Box2D;
	};

	// Meshes only when drawn: the check scenes never render.
	World BuildWorld(Scene& scene, Renderer3D* renderer)
	{
		auto mesh = [renderer](Entity entity, bool sphere, const glm::vec4& color)
		{
			if (renderer)
				entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(sphere ? renderer->GetSphereMesh() : renderer->GetBoxMesh(), color));
		};

		Entity ground = scene.CreateEntity("Ground");
		ground.AddComponent<Transform3DComponent>(Transform3DComponent({ 0.0f, -0.5f, 0.0f }, { 24.0f, 1.0f, 12.0f }));
		ground.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Static));
		ground.AddComponent<BoxCollider3DComponent>();
		mesh(ground, false, { 0.35f, 0.45f, 0.35f, 1.0f });

		Entity slope = scene.CreateEntity("Slope");
		Transform3DComponent& slopeTransform = slope.AddComponent<Transform3DComponent>(Transform3DComponent({ -6.0f, 1.2f, 0.0f }, { 8.0f, 0.3f, 3.0f }));
		slopeTransform.SetRotationEuler({ 0.0f, 0.0f, -18.0f });
		slope.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Static));
		slope.AddComponent<BoxCollider3DComponent>();
		mesh(slope, false, { 0.55f, 0.50f, 0.40f, 1.0f });

		World world;
		world.Ball = scene.CreateEntity("Ball");
		world.Ball.AddComponent<Transform3DComponent>(Transform3DComponent({ 2.0f, 6.0f, -2.0f }));
		world.Ball.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Dynamic));
		world.Ball.AddComponent<SphereCollider3DComponent>().Restitution = 0.6f;
		mesh(world.Ball, true, { 0.85f, 0.35f, 0.30f, 1.0f });

		world.Roller = scene.CreateEntity("Roller");
		world.Roller.AddComponent<Transform3DComponent>(Transform3DComponent({ -9.0f, 3.2f, 0.0f }));
		world.Roller.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Dynamic));
		world.Roller.AddComponent<SphereCollider3DComponent>();
		mesh(world.Roller, true, { 0.30f, 0.55f, 0.85f, 1.0f });

		Entity ground2D = scene.CreateEntity("Ground 2D");
		ground2D.GetComponent<TransformComponent>() = TransformComponent({ 0.0f, -0.5f, 0.0f }, { 20.0f, 1.0f });
		ground2D.AddComponent<RigidBody2DComponent>(RigidBody2DComponent(BodyType2D::Static));
		ground2D.AddComponent<BoxCollider2DComponent>();

		world.Box2D = scene.CreateEntity("Box 2D");
		world.Box2D.GetComponent<TransformComponent>() = TransformComponent({ 0.0f, 4.0f, 0.0f });
		world.Box2D.GetComponent<TransformComponent>().Rotation = 25.0f;
		world.Box2D.AddComponent<RigidBody2DComponent>(RigidBody2DComponent(BodyType2D::Dynamic));
		world.Box2D.AddComponent<BoxCollider2DComponent>().Restitution = 0.3f;

		return world;
	}

	struct Run
	{
		Probe Result;
		uint64_t Ticks = 0;
		double Elapsed = 0.0;
	};

	template<typename NextFrame>
	Run RunFor(bool fixedTick, double seconds, NextFrame nextFrame)
	{
		SceneParams params;
		params.FixedTick = fixedTick;
		Scene scene("Fixed Tick Check", params);
		World world = BuildWorld(scene, nullptr);
		Run run;
		world.Ball.AddScript<PushScript>(&run.Result, world.Roller, world.Box2D);
		scene.OnStart();

		double elapsed = 0.0;
		uint64_t frame = 0;
		while (elapsed < seconds)
		{
			const float dt = nextFrame(frame++);
			scene.OnUpdate(dt);
			elapsed += dt;
		}
		run.Ticks = scene.GetTick();
		run.Elapsed = elapsed;
		return run;
	}

	// Where a camera entity is drawn: its world translation, recovered from its view-projection and
	// that of an identical camera at the origin.
	glm::vec3 DrawnPosition(Scene& scene, Entity camera, Entity reference)
	{
		const glm::mat4 world = glm::inverse(scene.GetCameraViewProjection(camera, 1.0f)) * scene.GetCameraViewProjection(reference, 1.0f);
		return glm::vec3(world[3]);
	}

	Entity AddEye(Scene& scene, Entity parent, bool perspective)
	{
		Entity eye = scene.CreateEntity("Eye");
		CameraComponent& camera = eye.AddComponent<CameraComponent>();
		camera.Type = perspective ? CameraComponent::ProjectionType::Perspective : CameraComponent::ProjectionType::Orthographic;
		camera.Primary = false;
		if (perspective)
			eye.AddComponent<Transform3DComponent>();
		if (parent)
			eye.SetParent(parent, false);
		return eye;
	}

	bool Near(const glm::vec3& a, const glm::vec3& b, float tolerance = 1e-3f)
	{
		return glm::all(glm::lessThanEqual(glm::abs(a - b), glm::vec3(tolerance)));
	}
}

namespace Dingo
{

	void FixedTickTest::Initialize()
	{
		m_Checks.clear();
		RunDeterminismChecks();
		RunCounterChecks();
		RunInterpolationChecks();
		BuildShowcase();
	}

	void FixedTickTest::RunDeterminismChecks()
	{
		const float step = 1.0f / 60.0f;
		const Run at30 = RunFor(true, 10.0, [](uint64_t) { return 1.0f / 30.0f; });
		const Run at60 = RunFor(true, 10.0, [](uint64_t) { return 1.0f / 60.0f; });
		const Run at144 = RunFor(true, 10.0, [](uint64_t) { return 1.0f / 144.0f; });
		uint32_t seed = 12345u;
		const Run jitter = RunFor(true, 10.0, [&seed](uint64_t)
		{
			seed = seed * 1664525u + 1013904223u;
			return 1.0f / 240.0f + (seed >> 8) / float(1u << 24) * (1.0f / 20.0f - 1.0f / 240.0f);
		});
		const Run perFrame = RunFor(false, 600 * double(step) - 1e-4, [step](uint64_t) { return step; });

		Check(at30.Result.Recorded && at30.Result == at60.Result && at60.Result == at144.Result && at144.Result == jitter.Result,
			std::format("frames of 1/30, 1/60, 1/144 s and jittery ones leave 3D and 2D bodies bitwise equal at tick {}", k_RecordTick));
		Check(perFrame.Result == at144.Result,
			"each tick is one physics step of exactly the fixed step, with that tick's OnFixedUpdate forces (equal to a per-frame scene stepped at 1/60 s)");

		bool ticksClose = true;
		for (const Run* run : { &at30, &at60, &at144, &jitter })
		{
			const auto expected = static_cast<int64_t>(run->Elapsed * 60.0);
			const auto ticks = static_cast<int64_t>(run->Ticks);
			ticksClose = ticksClose && ticks >= expected - 1 && ticks <= expected + 1 && run->Result.Calls == run->Ticks;
		}
		Check(ticksClose, std::format("about 10 s of frames run one tick per 1/60 s simulated, one OnFixedUpdate each, give or take the last one's rounding ({}, {}, {}, {})",
			at30.Ticks, at60.Ticks, at144.Ticks, jitter.Ticks));
		Check(perFrame.Ticks == 0 && perFrame.Result.Calls == 600, "a scene without FixedTick counts no ticks and keeps its fixed steps");
	}

	void FixedTickTest::RunCounterChecks()
	{
		SceneParams params;
		params.FixedTick = true;
		{
			Scene scene("Tick Counter", params);
			World world = BuildWorld(scene, nullptr);
			scene.SetFixedTimeStep(1.0f / 64.0f);
			scene.OnStart();
			const bool zero = scene.GetTick() == 0;
			scene.OnUpdate(1.0f / 64.0f);
			const bool one = scene.GetTick() == 1 && scene.GetFixedStepsLastUpdate() == 1;
			const glm::vec3 before = world.Ball.GetComponent<Transform3DComponent>().Position;
			scene.SetTick(100);
			const bool set = scene.GetTick() == 100 && world.Ball.GetComponent<Transform3DComponent>().Position == before;
			scene.OnUpdate(1.0f / 64.0f);
			const bool next = scene.GetTick() == 101 && world.Ball.GetComponent<Transform3DComponent>().Position != before;
			Check(zero && one && set && next, "GetTick is 0 before the first tick and +1 per tick; SetTick changes the number and runs nothing");

			scene.OnStop();
			scene.OnStart();
			Check(scene.GetTick() == 0, "OnStart restarts the tick count");
		}
		{
			Scene scene("Tick Cap", params);
			BuildWorld(scene, nullptr);
			scene.SetFixedTimeStep(1.0f / 256.0f);
			scene.OnStart();
			scene.OnUpdate(1.0f / 16.0f);
			Check(scene.GetFixedStepsLastUpdate() == Scene::k_MaxFixedStepsPerUpdate && scene.GetTick() == 8 && scene.GetDroppedFixedSteps() == 8,
				std::format("an update owing 16 ticks runs {} and counts the 8 dropped (ran {}, dropped {})", Scene::k_MaxFixedStepsPerUpdate,
					scene.GetFixedStepsLastUpdate(), scene.GetDroppedFixedSteps()));
		}
	}

	void FixedTickTest::RunInterpolationChecks()
	{
		const float step = 1.0f / 64.0f;
		auto build = [step](bool interpolatePhysics)
		{
			SceneParams params;
			params.FixedTick = true;
			params.InterpolatePhysics = interpolatePhysics;
			Scene* scene = new Scene("Interpolation", params);
			scene->SetFixedTimeStep(step);
			return scene;
		};

		Scene* scene = build(true);
		Entity ball = scene->CreateEntity("Ball");
		ball.AddComponent<Transform3DComponent>(Transform3DComponent({ 0.0f, 5.0f, 0.0f }));
		ball.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Dynamic));
		ball.AddComponent<SphereCollider3DComponent>();
		Entity stiff = scene->CreateEntity("Stiff Ball");
		stiff.AddComponent<Transform3DComponent>(Transform3DComponent({ 0.0f, 5.0f, 4.0f }));
		stiff.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Dynamic)).Interpolate = false;
		stiff.AddComponent<SphereCollider3DComponent>();
		Entity box = scene->CreateEntity("Box 2D");
		box.AddComponent<RigidBody2DComponent>(RigidBody2DComponent(BodyType2D::Dynamic));
		box.AddComponent<BoxCollider2DComponent>();

		const Entity eye = AddEye(*scene, ball, true);
		const Entity stiffEye = AddEye(*scene, stiff, true);
		const Entity reference = AddEye(*scene, {}, true);
		const Entity eye2D = AddEye(*scene, box, false);
		const Entity reference2D = AddEye(*scene, {}, false);

		scene->OnStart();
		scene->SetLinearVelocity(ball, glm::vec3(8.0f, 0.0f, 0.0f));
		scene->SetLinearVelocity(stiff, glm::vec3(8.0f, 0.0f, 0.0f));
		scene->SetLinearVelocity(box, glm::vec2(8.0f, 0.0f));

		auto position = [](Entity entity) { return entity.GetComponent<Transform3DComponent>().Position; };
		auto position2D = [](Entity entity) { return entity.GetComponent<TransformComponent>().Position; };

		scene->OnUpdate(step);
		scene->OnUpdate(step);
		const glm::vec3 previous = position(ball);
		const glm::vec3 previous2D = position2D(box);
		scene->OnUpdate(step);
		const glm::vec3 latest = position(ball);
		const glm::vec3 latest2D = position2D(box);
		scene->OnUpdate(step * 0.5f);

		const float alpha = scene->GetFixedTimeAlpha();
		const glm::vec3 drawn = DrawnPosition(*scene, eye, reference);
		const glm::vec3 drawn2D = DrawnPosition(*scene, eye2D, reference2D);
		Check(alpha == 0.5f && Near(drawn, glm::mix(previous, latest, 0.5f)) && !Near(drawn, latest) && Near(drawn2D, glm::mix(previous2D, latest2D, 0.5f))
			&& !Near(drawn2D, latest2D), "at alpha 0.5 a 3D and a 2D body, and a child camera, are drawn halfway between their last two ticks");
		Check(position(ball) == latest && position2D(box) == latest2D, "outside drawing, the components hold the latest tick");
		Check(Near(DrawnPosition(*scene, stiffEye, reference), position(stiff)), "RigidBody3DComponent::Interpolate = false draws the latest tick");

		Physics3D* physics = scene->GetPhysics3D();
		physics->SetPosition(scene->GetRuntimeBody3D(ball), glm::vec3(20.0f, 5.0f, 0.0f));
		const bool heldAtLatest = Near(DrawnPosition(*scene, eye, reference), latest);
		scene->OnUpdate(step * 0.5f);
		scene->OnUpdate(step * 0.5f);
		Check(heldAtLatest && scene->GetFixedTimeAlpha() == 0.5f && position(ball).x > 19.0f && Near(DrawnPosition(*scene, eye, reference), position(ball)),
			"a teleported body snaps to its new place instead of sliding there");

		ball.GetComponent<Transform3DComponent>().Position = glm::vec3(-5.0f, 1.0f, 0.0f);
		Check(Near(DrawnPosition(*scene, eye, reference), glm::vec3(-5.0f, 1.0f, 0.0f)), "a transform the game wrote since the last tick is drawn as written");

		scene->OnUpdate(step * 0.5f);
		scene->OnUpdate(step);
		const glm::vec3 before = position(ball);
		scene->OnUpdate(step);
		const glm::vec3 after = position(ball);
		scene->OnUpdate(step * 0.5f);
		const glm::vec3 halfway = glm::mix(before, after, 0.5f);
		const bool drawnHalfway = Near(glm::vec3(scene->GetDrawnWorldTransform(ball)[3]), halfway) && !Near(halfway, after);

		const glm::quat facing = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		ball.GetComponent<Transform3DComponent>().Rotation = facing;
		physics->SetRotation(scene->GetRuntimeBody3D(ball), facing);
		const glm::mat4 turned = scene->GetDrawnWorldTransform(ball);
		const glm::vec3 turnedForward = glm::vec3(turned * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));
		const glm::vec3 facingForward = facing * glm::vec3(0.0f, 0.0f, -1.0f);
		Check(scene->GetFixedTimeAlpha() == 0.5f && drawnHalfway && Near(glm::vec3(turned[3]), halfway) && Near(turnedForward, facingForward),
			"GetDrawnWorldTransform reads the drawn pose, and a rotation the game set keeps the position smooth and is drawn as set");
		delete scene;

		scene = build(false);
		ball = scene->CreateEntity("Ball");
		ball.AddComponent<Transform3DComponent>(Transform3DComponent({ 0.0f, 5.0f, 0.0f }));
		ball.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Dynamic));
		ball.AddComponent<SphereCollider3DComponent>();
		const Entity plainEye = AddEye(*scene, ball, true);
		const Entity plainReference = AddEye(*scene, {}, true);
		scene->OnStart();
		scene->SetLinearVelocity(ball, glm::vec3(8.0f, 0.0f, 0.0f));
		scene->OnUpdate(step);
		scene->OnUpdate(step);
		scene->OnUpdate(step * 0.5f);
		Check(Near(DrawnPosition(*scene, plainEye, plainReference), position(ball)), "SceneParams::InterpolatePhysics = false draws the latest tick");
		delete scene;
	}

	void FixedTickTest::BuildShowcase()
	{
		SceneParams params;
		params.FixedTick = true;
		params.InterpolatePhysics = m_Interpolate;
		m_Scene = new Scene("Fixed Tick Showcase", params);
		m_Scene->SetFixedTimeStep(1.0f / k_TickRates[m_TickRate]);

		Entity sun = m_Scene->CreateEntity("Sun");
		sun.AddComponent<DirectionalLightComponent>();

		World world = BuildWorld(*m_Scene, &Application::Get().GetRenderer3D());
		m_Scene->OnStart();
		m_Scene->SetLinearVelocity(world.Ball, glm::vec3(-1.5f, 0.0f, 1.0f));
		m_ResetTimer = 0.0f;
	}

	void FixedTickTest::Update(float deltaTime)
	{
		m_ResetTimer += deltaTime;
		if (m_AutoReset && m_ResetTimer > 8.0f)
		{
			delete m_Scene;
			BuildShowcase();
		}

		m_Scene->OnUpdate(deltaTime);

		m_Camera.SetPosition({ 0.0f, 6.0f, 16.0f });
		m_Camera.SetTarget({ -2.0f, 1.0f, 0.0f });

		Renderer3D& renderer = Application::Get().GetRenderer3D();
		renderer.BeginScene(m_Camera);
		renderer.Clear(m_ClearColor);
		m_Scene->SubmitLights(renderer);
		m_Scene->RenderEntities3D(renderer);
		renderer.EndScene();
	}

	void FixedTickTest::Cleanup()
	{
		delete m_Scene;
		m_Scene = nullptr;
	}

	void FixedTickTest::Resize(uint32_t width, uint32_t height)
	{
		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
		m_Camera.SetAspectRatio(m_AspectRatio);
	}

	void FixedTickTest::ImGuiRender()
	{
		GraphicsTest::ImGuiRender();
		ImGui::Separator();

		bool rebuild = ImGui::Combo("Tick rate", &m_TickRate, k_TickRateNames, IM_ARRAYSIZE(k_TickRateNames));
		rebuild |= ImGui::Checkbox("Interpolate physics", &m_Interpolate);
		ImGui::Checkbox("Restart every 8 s", &m_AutoReset);
		if (ImGui::Button("Restart") || rebuild)
		{
			delete m_Scene;
			BuildShowcase();
		}
		ImGui::Text("Tick %llu, alpha %.2f, %u this update, %llu dropped", static_cast<unsigned long long>(m_Scene->GetTick()),
			m_Scene->GetFixedTimeAlpha(), m_Scene->GetFixedStepsLastUpdate(), static_cast<unsigned long long>(m_Scene->GetDroppedFixedSteps()));

		ImGui::Separator();
		for (const CheckResult& check : m_Checks)
		{
			const ImVec4 color = check.Passed ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f) : ImVec4(0.95f, 0.3f, 0.3f, 1.0f);
			ImGui::TextColored(color, "%s %s", check.Passed ? "[PASS]" : "[FAIL]", check.Name.c_str());
		}
	}

}
