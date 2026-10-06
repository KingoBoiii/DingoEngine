#include "HierarchyTest.h"

#include <glm/gtc/constants.hpp>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <format>

namespace
{
	using namespace Dingo;

	constexpr float k_Tolerance = 1e-4f;
	constexpr float k_PhysicsCheckSeconds = 3.0f;

	bool Near(const glm::vec3& a, const glm::vec3& b, float tolerance = k_Tolerance)
	{
		return glm::length(a - b) <= tolerance;
	}

	bool Near(const glm::quat& a, const glm::quat& b, float tolerance = k_Tolerance)
	{
		return std::abs(glm::dot(a, b)) >= 1.0f - tolerance;
	}

	bool Near(const glm::mat4& a, const glm::mat4& b, float tolerance = k_Tolerance)
	{
		for (int column = 0; column < 4; column++)
			for (int row = 0; row < 4; row++)
				if (std::abs(a[column][row] - b[column][row]) > tolerance)
					return false;
		return true;
	}

	Entity MakeEntity(Scene& scene, const std::string& name, const glm::vec3& position, const glm::vec3& eulerDegrees, const glm::vec3& scale)
	{
		Entity entity = scene.CreateEntity(name);
		entity.AddComponent<Transform3DComponent>(Transform3DComponent(position, scale)).SetRotationEuler(eulerDegrees);
		return entity;
	}

	Entity MakeBox(Scene& scene, const std::string& name, const glm::vec3& position, const glm::vec3& scale, BodyType3D type)
	{
		Entity entity = MakeEntity(scene, name, position, glm::vec3(0.0f), scale);
		entity.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(type));
		entity.AddComponent<BoxCollider3DComponent>();
		return entity;
	}

	// One second in fixed 1/60 s steps, so the filter checks don't depend on the frame rate.
	void StepOneSecond(Scene& scene)
	{
		for (int i = 0; i < 60; i++)
			scene.OnUpdate(1.0f / 60.0f);
	}

	// "name>parent name", in the order OnDestroy ran.
	class DestroyRecorder : public ScriptableEntity
	{
	public:
		explicit DestroyRecorder(std::vector<std::string>* log) : m_Log(log) {}

	protected:
		void OnDestroy() override
		{
			const Entity parent = GetEntity().GetParent();
			m_Log->push_back(GetEntity().GetName() + ">" + (parent ? parent.GetName() : std::string()));
		}

	private:
		std::vector<std::string>* m_Log;
	};

	class DestroyOnDestroy : public ScriptableEntity
	{
	public:
		explicit DestroyOnDestroy(Entity target) : m_Target(target) {}

	protected:
		void OnDestroy() override { GetScene().DestroyEntity(m_Target); }

	private:
		Entity m_Target;
	};

	class AdoptOnDestroy : public ScriptableEntity
	{
	public:
		explicit AdoptOnDestroy(Entity* adopted) : m_Adopted(adopted) {}

	protected:
		void OnDestroy() override
		{
			*m_Adopted = GetScene().CreateEntity("Smoke");
			m_Adopted->SetParent(GetEntity(), false);
		}

	private:
		Entity* m_Adopted;
	};

	class DestroyInUpdate : public ScriptableEntity
	{
	public:
		DestroyInUpdate(Entity target, Entity child, bool* aliveAfterCall)
			: m_Target(target), m_Child(child), m_AliveAfterCall(aliveAfterCall) {}

	protected:
		void OnUpdate(float) override
		{
			if (m_Done)
				return;

			m_Done = true;
			GetScene().DestroyEntity(m_Target);
			*m_AliveAfterCall = m_Target.IsValid() && m_Child.IsValid();
		}

	private:
		Entity m_Target;
		Entity m_Child;
		bool* m_AliveAfterCall;
		bool m_Done = false;
	};
}

namespace Dingo
{

	void HierarchyTest::Check(bool condition, const std::string& name)
	{
		m_Checks.push_back({ name, condition });
		if (condition)
			DE_INFO("[PASS] {}", name);
		else
			DE_ERROR("[FAIL] {}", name);
	}

	void HierarchyTest::Initialize()
	{
		m_Checks.clear();
		m_Time = 0.0f;
		m_LightProbeDone = false;
		m_PhysicsChecksDone = false;
		m_MaxCrateBodyGap = 0.0f;
		m_MaxPaddleBodyGap = 0.0f;
		m_MaxPaddleAngleGap = 0.0f;
		m_PaddleTravel = 0.0f;
		m_MaxRiderGap = 0.0f;
		m_MaxRiderAngleGap = 0.0f;
		m_PlatformTravel = 0.0f;
		m_MaxFallerGap = 0.0f;
		m_MaxWalkerGap = 0.0f;
		m_WalkerCarrierTravel = 0.0f;

		m_MaxCrate2DGap = 0.0f;
		m_Carrier2DTravel = 0.0f;
		m_MaxPaddle2DGap = 0.0f;
		m_MaxPaddle2DAngleGap = 0.0f;
		m_MaxRider2DGap = 0.0f;
		m_MaxRider2DAngleGap = 0.0f;
		m_Platform2DTravel = 0.0f;
		m_Paddle2DTravel = 0.0f;
		m_MaxFaller2DGap = 0.0f;

		// --hierarchy=2d shows the 2D section, --hierarchy=probe2d frames the muzzle sprite at the
		// centre, --hierarchy=stress / stressflat time 10k entities parented / flat.
		if (auto view = Application::Get().GetCommandLineArgs().Get("hierarchy"))
		{
			m_View = *view == "2d" ? View::Scene2D : *view == "probe2d" ? View::Probe2D
				: *view == "stress" ? View::Stress : *view == "stressflat" ? View::StressFlat : View::Scene3D;
		}

		RunStructuralChecks();
		RunCollisionFilterChecks();
		RunStructuralChecks2D();
		BuildScene();
		BuildScene2D();
		if (m_View == View::Stress || m_View == View::StressFlat)
			BuildStressScene(m_View == View::Stress);

		m_Camera = PerspectiveCamera(45.0f, m_AspectRatio, 0.1f, 200.0f);
	}

	void HierarchyTest::RunStructuralChecks()
	{
		std::vector<std::string> destroyLog;
		bool aliveAfterDeferredDestroy = false;

		Scene scene("Hierarchy Checks");

		Entity a = MakeEntity(scene, "A", { 1.0f, 2.0f, 3.0f }, { 10.0f, 40.0f, -20.0f }, glm::vec3(2.0f));
		Entity b = MakeEntity(scene, "B", { -2.0f, 0.5f, 1.0f }, { 0.0f, -30.0f, 15.0f }, glm::vec3(0.5f));
		Entity c = MakeEntity(scene, "C", { 0.0f, 1.0f, -4.0f }, { 25.0f, 0.0f, 60.0f }, { 1.0f, 3.0f, 1.0f });
		b.SetParent(a, false);
		c.SetParent(b, false);

		{
			const Transform3DComponent& la = a.GetComponent<Transform3DComponent>();
			const Transform3DComponent& lb = b.GetComponent<Transform3DComponent>();
			const Transform3DComponent& lc = c.GetComponent<Transform3DComponent>();

			Check(a.GetWorldTransform() == la.GetTransform() && a.GetWorldPosition() == la.Position
				&& a.GetWorldRotation() == la.Rotation && a.GetWorldScale() == la.Scale,
				"a root's world transform is exactly its own Transform3DComponent");

			const glm::mat4 expected = la.GetTransform() * lb.GetTransform() * lc.GetTransform();
			Check(Near(c.GetWorldTransform(), expected) && Near(c.GetWorldPosition(), glm::vec3(expected[3]))
				&& Near(c.GetWorldRotation(), la.Rotation * lb.Rotation * lc.Rotation)
				&& Near(c.GetWorldScale(), la.Scale * lb.Scale * lc.Scale),
				"world = parent x local down a three-level chain (position, rotation, scale)");

			Check(c.GetWorldTransform() == b.GetWorldTransform() * lc.GetTransform(),
				"a child's world is exactly its parent's world x its local");
		}

		Entity d = MakeEntity(scene, "D", { 4.0f, -1.0f, 2.0f }, { 0.0f, 70.0f, 10.0f }, { 1.5f, 1.0f, 0.5f });
		const glm::mat4 dWorld = d.GetWorldTransform();
		d.SetParent(b);
		Check(d.GetParent() == b && Near(d.GetWorldTransform(), dWorld, 1e-3f)
			&& !Near(d.GetComponent<Transform3DComponent>().Position, { 4.0f, -1.0f, 2.0f }),
			"SetParent(keepWorldTransform = true) keeps the world transform and rewrites the local one");

		Entity e = MakeEntity(scene, "E", { 0.5f, 0.0f, -1.0f }, { 0.0f, 45.0f, 0.0f }, glm::vec3(1.0f));
		const Transform3DComponent eBefore = e.GetComponent<Transform3DComponent>();
		e.SetParent(c, false);
		{
			const Transform3DComponent& le = e.GetComponent<Transform3DComponent>();
			Check(le.Position == eBefore.Position && le.Rotation == eBefore.Rotation && le.Scale == eBefore.Scale
				&& Near(e.GetWorldTransform(), c.GetWorldTransform() * eBefore.GetTransform()),
				"SetParent(keepWorldTransform = false) keeps the local values, so the entity moves under its new parent");
		}

		Entity f = MakeEntity(scene, "F", { -3.0f, 2.0f, 5.0f }, { 0.0f, -90.0f, 0.0f }, glm::vec3(1.0f));
		const glm::mat4 cWorld = c.GetWorldTransform();
		const glm::mat4 eWorld = e.GetWorldTransform();
		c.SetParent(f);
		Check(c.GetParent() == f && b.GetChildCount() == 1 && b.GetChildren().front() == d && f.GetChildCount() == 1
			&& Near(c.GetWorldTransform(), cWorld, 1e-3f) && Near(e.GetWorldTransform(), eWorld, 1e-3f),
			"reparenting a subtree from depth 2 to depth 1 keeps it in place and updates both parents");

		c.RemoveParent();
		Check(!c.GetParent() && f.GetChildCount() == 0 && Near(c.GetWorldTransform(), cWorld, 1e-3f),
			"RemoveParent(keepWorldTransform = true) leaves the entity where it was, as a root");

		{
			Entity folder = scene.CreateEntity("Folder");
			Entity item = MakeEntity(scene, "Folder Item", { 1.0f, 0.0f, 0.0f }, glm::vec3(0.0f), glm::vec3(1.0f));
			item.SetParent(folder, false);
			Entity shelf = MakeEntity(scene, "Shelf", { 0.0f, 5.0f, 0.0f }, glm::vec3(0.0f), glm::vec3(1.0f));
			folder.SetParent(shelf);
			Check(Near(item.GetWorldPosition(), { 1.0f, 5.0f, 0.0f }),
				"keepWorldTransform on a parent without a Transform3DComponent moves its 3D children (one warning logged)");

			shelf.GetComponent<Transform3DComponent>().Position = { 2.0f, 6.0f, -1.0f };
			folder.SetWorldPosition({ 9.0f, 9.0f, 9.0f });
			Check(folder.GetWorldPosition() == glm::vec3(2.0f, 6.0f, -1.0f) && Near(item.GetWorldPosition(), { 3.0f, 6.0f, -1.0f })
				&& folder.GetComponent<TransformComponent>().Position == glm::vec3(0.0f),
				"a grouping node without a Transform3DComponent reports its moving parent's 3D world and ignores world writes");
		}

		{
			const Entity null;
			Entity stray = MakeEntity(scene, "Stray", { 1.0f, 2.0f, 3.0f }, glm::vec3(0.0f), glm::vec3(1.0f));
			scene.DestroyEntity(stray);
			stray.SetWorldPosition({ 4.0f, 5.0f, 6.0f });
			Check(null.GetWorldTransform() == glm::mat4(1.0f) && null.GetWorldPosition() == glm::vec3(0.0f)
				&& null.GetWorldRotation() == glm::quat(1.0f, 0.0f, 0.0f, 0.0f) && null.GetWorldScale() == glm::vec3(1.0f)
				&& stray.GetWorldPosition() == glm::vec3(0.0f) && !null.GetParent() && null.GetChildCount() == 0,
				"a null or destroyed entity reads as an identity root and ignores world writes");
		}

		{
			Entity zero = MakeEntity(scene, "Flat", { 0.0f, 0.0f, 0.0f }, glm::vec3(0.0f), { 1.0f, 0.0f, 1.0f });
			Entity onFlat = MakeEntity(scene, "On Flat", { 2.0f, 3.0f, 4.0f }, { 0.0f, 30.0f, 0.0f }, glm::vec3(1.0f));
			onFlat.SetParent(zero);
			const Transform3DComponent& kept = onFlat.GetComponent<Transform3DComponent>();
			const bool keptLocal = kept.Position == glm::vec3(2.0f, 3.0f, 4.0f) && kept.Scale == glm::vec3(1.0f);
			onFlat.SetWorldPosition({ 9.0f, 9.0f, 9.0f });
			const glm::vec3 position = onFlat.GetComponent<Transform3DComponent>().Position;
			Check(keptLocal && position == glm::vec3(2.0f, 3.0f, 4.0f) && !std::isnan(onFlat.GetWorldPosition().x),
				"under a parent with a zero-scale axis, keepWorldTransform and SetWorldPosition keep the old local");
		}

		{
			Scene other("Other");
			Entity stranger = other.CreateEntity("Stranger");

			a.SetParent(d);
			const bool rejectedDescendant = !a.GetParent();
			b.SetParent(b);
			const bool rejectedSelf = b.GetParent() == a;
			b.SetParent(stranger);
			const bool rejectedForeign = b.GetParent() == a;
			Check(rejectedDescendant && rejectedSelf && rejectedForeign,
				"a cycle, a self-parent and a parent from another scene are rejected (three errors logged)");
		}

		{
			Entity list = scene.CreateEntity("List");
			std::vector<Entity> items;
			for (int i = 0; i < 5; i++)
			{
				items.push_back(scene.CreateEntity(std::format("Item {}", i)));
				items.back().SetParent(list);
			}
			items[1].SetParent(a);
			items[1].SetParent(list);
			Entity deep = scene.CreateEntity("Deep");
			deep.SetParent(items[3]);

			const std::vector<Entity> expected{ items[0], items[2], items[3], items[4], items[1] };
			Check(list.GetChildren() == expected && list.FindChild("Deep") == deep && !list.FindChild("Deep", false)
				&& list.FindChild("Item 2", false) == items[2] && list.GetChildCount() == 5,
				"children keep insertion order, and FindChild searches nearest first");

			uint32_t visited = 0;
			list.ForEachChild([&](Entity child)
			{
				visited++;
				scene.DestroyEntity(child);
			});
			Check(visited == 5 && list.GetChildCount() == 0 && !deep.IsValid(),
				"ForEachChild visits every child even when it destroys each one");
		}

		{
			Entity root = scene.CreateEntity("Root");
			Entity mid = scene.CreateEntity("Mid");
			Entity leafA = scene.CreateEntity("Leaf A");
			Entity leafB = scene.CreateEntity("Leaf B");
			Entity sibling = scene.CreateEntity("Sibling");
			mid.SetParent(root);
			leafA.SetParent(mid);
			leafB.SetParent(mid);
			sibling.SetParent(root);
			for (Entity entity : { root, mid, leafA, leafB, sibling })
				entity.AddScript<DestroyRecorder>(&destroyLog);

			scene.DestroyEntity(root);
			const std::vector<std::string> expected{ "Leaf A>Mid", "Leaf B>Mid", "Mid>Root", "Sibling>Root", "Root>" };
			Check(!root.IsValid() && !mid.IsValid() && !leafA.IsValid() && !leafB.IsValid() && !sibling.IsValid() && destroyLog == expected,
				"DestroyEntity takes the subtree children first, each OnDestroy still seeing its parent");
		}

		{
			Entity parent = scene.CreateEntity("Parent");
			Entity first = scene.CreateEntity("First");
			Entity second = scene.CreateEntity("Second");
			first.SetParent(parent);
			second.SetParent(parent);
			scene.DestroyEntity(first);
			Check(parent.GetChildCount() == 1 && parent.GetChildren().front() == second && second.GetParent() == parent,
				"destroying a child unlinks it from its parent");
		}

		{
			Entity smoke;
			Entity host = scene.CreateEntity("Host");
			host.AddScript<AdoptOnDestroy>(&smoke);
			scene.DestroyEntity(host);
			Check(!host.IsValid() && smoke != Entity() && !smoke.IsValid(),
				"a child parented to an entity inside its own OnDestroy is destroyed with it");
		}

		{
			Entity tree = scene.CreateEntity("Tree");
			Entity branch = scene.CreateEntity("Branch");
			Entity twig = scene.CreateEntity("Twig");
			branch.SetParent(tree);
			twig.SetParent(branch);
			Entity trigger = scene.CreateEntity("Trigger");
			trigger.AddScript<DestroyOnDestroy>(tree);

			scene.DestroyEntity(trigger);
			Check(!trigger.IsValid() && !tree.IsValid() && !branch.IsValid() && !twig.IsValid(),
				"a DestroyEntity from a script's OnDestroy takes that entity's subtree too");
		}

		{
			Entity doomed = scene.CreateEntity("Doomed");
			Entity doomedChild = scene.CreateEntity("Doomed Child");
			doomedChild.SetParent(doomed);
			Entity runner = scene.CreateEntity("Runner");
			runner.AddScript<DestroyInUpdate>(doomed, doomedChild, &aliveAfterDeferredDestroy);

			scene.OnUpdate(1.0f / 60.0f);
			Check(aliveAfterDeferredDestroy && !doomed.IsValid() && !doomedChild.IsValid(),
				"a destroy from OnUpdate defers the whole subtree to the end of the pass");
			scene.DestroyEntity(runner);
		}

		{
			Entity solar = MakeEntity(scene, "System", { 0.0f, 1.0f, 0.0f }, { 0.0f, 30.0f, 0.0f }, glm::vec3(1.5f));
			Entity planet = MakeEntity(scene, "Planet", { 3.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 20.0f }, glm::vec3(0.5f));
			Entity moon = MakeEntity(scene, "Moon", { 1.0f, 0.5f, 0.0f }, { 0.0f, 0.0f, 0.0f }, glm::vec3(0.3f));
			planet.SetParent(solar, false);
			moon.SetParent(planet, false);

			Entity copy = scene.DuplicateEntity(planet);
			Entity moonCopy = copy.FindChild("Moon", false);
			Check(copy && copy != planet && copy.GetParent() == solar && solar.GetChildCount() == 2 && solar.GetChildren().back() == copy
				&& copy.GetChildCount() == 1 && moonCopy && moonCopy != moon && moonCopy.GetUUID() != moon.GetUUID()
				&& planet.GetChildCount() == 1 && Near(moonCopy.GetWorldTransform(), moon.GetWorldTransform()),
				"DuplicateEntity copies the subtree and gives the copy the source's parent");
		}

		{
			Entity rig = MakeEntity(scene, "Rig", { 2.0f, 3.0f, 4.0f }, { 0.0f, 60.0f, 0.0f }, glm::vec3(1.0f));
			Entity eye = MakeEntity(scene, "Eye", { 0.0f, 1.0f, 5.0f }, { -20.0f, 0.0f, 0.0f }, glm::vec3(1.0f));
			eye.SetParent(rig, false);
			CameraComponent& camera = eye.AddComponent<CameraComponent>();
			camera.Type = CameraComponent::ProjectionType::Perspective;

			const glm::vec3 position = eye.GetWorldPosition();
			const glm::quat rotation = eye.GetWorldRotation();
			const glm::vec3 forward = rotation * glm::vec3(0.0f, 0.0f, -1.0f);
			const float aspect = 16.0f / 9.0f;
			const glm::mat4 expected = camera.GetProjection(aspect)
				* glm::inverse(glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation));
			const Ray ray = scene.ScreenPointToRay({ 800.0f, 450.0f }, { 1600.0f, 900.0f });
			Check(Near(scene.GetCameraViewProjection(eye, aspect), expected, 1e-3f) && glm::dot(ray.Direction, forward) > 0.9999f
				&& Near(ray.Origin, position + forward * camera.PerspNear, 1e-2f) && !Near(position, { 0.0f, 1.0f, 5.0f }, 0.5f),
				"a camera on a child views and picks from its world transform");
		}
	}

	void HierarchyTest::RunCollisionFilterChecks()
	{
		bool reportedWhileParented = false;
		bool clearedAfterDetach = false;

		{
			// A blade socketed to a hand, overlapping the body the hand hangs off.
			Scene scene("Ancestor Filter");
			Entity floor = MakeBox(scene, "Floor", { 0.0f, -0.5f, 0.0f }, { 8.0f, 1.0f, 8.0f }, BodyType3D::Static);
			Entity body = MakeBox(scene, "Body", { 0.0f, 0.5f, 0.0f }, glm::vec3(1.0f), BodyType3D::Dynamic);
			Entity hand = MakeEntity(scene, "Hand", { 0.5f, 0.0f, 0.0f }, glm::vec3(0.0f), glm::vec3(1.0f));
			hand.SetParent(body, false);
			Entity blade = MakeBox(scene, "Blade", { 0.1f, 0.0f, 0.0f }, { 0.6f, 0.2f, 0.2f }, BodyType3D::Kinematic);
			blade.SetParent(hand, false);
			scene.OnStart();

			Physics3D* physics = scene.GetPhysics3D();
			if (!physics)
			{
				Check(false, "the collision-filter scene has a 3D world");
				return;
			}

			const PhysicsBodyId3D bodyId = scene.GetRuntimeBody3D(body);
			const PhysicsBodyId3D bladeId = scene.GetRuntimeBody3D(blade);
			const glm::vec3 start = physics->GetPosition(bodyId);
			StepOneSecond(scene);
			const float moved = glm::length(physics->GetPosition(bodyId) - start);
			reportedWhileParented = physics->IsCollisionIgnored(bladeId, bodyId) && physics->IsCollisionIgnored(bodyId, bladeId)
				&& !physics->IsCollisionIgnored(bladeId, scene.GetRuntimeBody3D(floor));
			Check(moved < 1e-3f, std::format("a kinematic child doesn't shove the dynamic body it hangs off, resting on the floor (moved {:.1e})", moved));

			blade.RemoveParent();
			const glm::vec3 detached = physics->GetPosition(bodyId);
			StepOneSecond(scene);
			const float pushed = glm::length(physics->GetPosition(bodyId) - detached);
			clearedAfterDetach = !physics->IsCollisionIgnored(bladeId, bodyId);
			Check(pushed > 0.05f, std::format("after RemoveParent the kinematic box pushes its former ancestor's body again (moved {:.2f})", pushed));
		}

		{
			// No floor and no velocity: only the shield's overlap could move the controller.
			Scene scene("Controller Filter");
			Entity walker = MakeEntity(scene, "Walker", { 0.0f, 2.0f, 0.0f }, glm::vec3(0.0f), glm::vec3(1.0f));
			walker.AddComponent<CharacterController3DComponent>();
			Entity shield = MakeBox(scene, "Shield", { 0.4f, 0.9f, 0.0f }, glm::vec3(0.4f), BodyType3D::Kinematic);
			shield.SetParent(walker, false);
			scene.OnStart();

			CharacterController3D* controller = scene.GetCharacterController(walker);
			if (!controller)
			{
				Check(false, "the collision-filter scene has a character controller");
				return;
			}

			const PhysicsBodyId3D shieldId = scene.GetRuntimeBody3D(shield);
			const glm::vec3 start = controller->GetPosition();
			StepOneSecond(scene);
			const float moved = glm::length(controller->GetPosition() - start);
			reportedWhileParented = reportedWhileParented && controller->IsBodyIgnored(shieldId);
			Check(moved < 1e-3f, std::format("a kinematic child doesn't shove its character-controller parent (moved {:.1e})", moved));

			shield.RemoveParent();
			const glm::vec3 detached = controller->GetPosition();
			StepOneSecond(scene);
			const float pushed = glm::length(controller->GetPosition() - detached);
			clearedAfterDetach = clearedAfterDetach && !controller->IsBodyIgnored(shieldId);
			Check(pushed > 0.05f, std::format("after RemoveParent the character controller is pushed out of its former kinematic child (moved {:.2f})", pushed));
		}

		{
			// The crate shares the blade's parent but is not its ancestor, so the blade still pushes it.
			Scene scene("Sibling Filter");
			MakeBox(scene, "Floor", { 0.0f, -0.5f, 0.0f }, { 8.0f, 1.0f, 8.0f }, BodyType3D::Static);
			Entity rack = MakeEntity(scene, "Rack", glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f));
			Entity crate = MakeBox(scene, "Crate", { 0.0f, 0.5f, 0.0f }, glm::vec3(1.0f), BodyType3D::Dynamic);
			crate.SetParent(rack, false);
			Entity blade = MakeBox(scene, "Blade", { 0.6f, 0.5f, 0.0f }, { 0.6f, 0.2f, 0.2f }, BodyType3D::Kinematic);
			blade.SetParent(rack, false);
			scene.OnStart();

			Physics3D* physics = scene.GetPhysics3D();
			if (!physics)
			{
				Check(false, "the collision-filter scene has a 3D world");
				return;
			}

			const PhysicsBodyId3D crateId = scene.GetRuntimeBody3D(crate);
			const glm::vec3 start = physics->GetPosition(crateId);
			StepOneSecond(scene);
			const float pushed = glm::length(physics->GetPosition(crateId) - start);
			Check(pushed > 0.05f && !physics->IsCollisionIgnored(scene.GetRuntimeBody3D(blade), crateId),
				std::format("a kinematic child still pushes a dynamic sibling, which is not its ancestor (moved {:.2f})", pushed));
		}

		Check(reportedWhileParented && clearedAfterDetach,
			"IsCollisionIgnored and IsBodyIgnored report a kinematic child's ancestor pairs while it is parented, and not after RemoveParent");
	}

	void HierarchyTest::BuildScene()
	{
		Renderer3D& renderer = Application::Get().GetRenderer3D();
		m_HullMesh = Mesh::CreateBox(2.4f, 0.8f, 3.2f);
		m_PlatformMesh = Mesh::CreateBox(2.0f, 0.4f, 2.0f);
		m_TurretMesh = Mesh::CreateBox(1.4f, 0.6f, 1.4f);
		m_BarrelMesh = Mesh::CreateBox(0.25f, 0.25f, 1.6f);

		m_Scene = new Scene("Hierarchy Test");

		Entity sun = m_Scene->CreateEntity("Sun Light");
		DirectionalLightComponent& sunLight = sun.AddComponent<DirectionalLightComponent>();
		sunLight.Intensity = 0.7f;
		sunLight.Ambient = 0.25f;

		Entity floor = MakeEntity(*m_Scene, "Floor", { 0.0f, -0.5f, 0.0f }, glm::vec3(0.0f), { 24.0f, 1.0f, 24.0f });
		floor.AddComponent<MeshRendererComponent>(MeshRendererComponent(renderer.GetBoxMesh(), { 0.45f, 0.48f, 0.44f, 1.0f }));
		floor.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Static));
		floor.AddComponent<BoxCollider3DComponent>();

		m_Sun = MakeEntity(*m_Scene, "Sun", { 0.0f, 3.5f, 0.0f }, glm::vec3(0.0f), glm::vec3(1.4f));
		m_Sun.AddComponent<MeshRendererComponent>(MeshRendererComponent(renderer.GetSphereMesh(), { 1.0f, 0.82f, 0.25f, 1.0f }));
		m_Planet = MakeEntity(*m_Scene, "Planet", { 3.0f, 0.0f, 0.0f }, glm::vec3(0.0f), glm::vec3(0.4f));
		m_Planet.AddComponent<MeshRendererComponent>(MeshRendererComponent(renderer.GetSphereMesh(), { 0.25f, 0.55f, 0.95f, 1.0f }));
		m_Planet.SetParent(m_Sun, false);
		Entity moon = MakeEntity(*m_Scene, "Moon", { 1.6f, 0.3f, 0.0f }, glm::vec3(0.0f), glm::vec3(0.4f));
		moon.AddComponent<MeshRendererComponent>(MeshRendererComponent(renderer.GetSphereMesh(), { 0.8f, 0.8f, 0.82f, 1.0f }));
		moon.SetParent(m_Planet, false);
		Entity planetLight = m_Scene->CreateEntity("Planet Light");
		planetLight.AddComponent<Transform3DComponent>();
		planetLight.AddComponent<PointLightComponent>(PointLightComponent({ 1.0f, 0.55f, 0.2f }, 2.5f, 5.5f));
		planetLight.SetParent(m_Planet, false);

		m_Hull = MakeEntity(*m_Scene, "Hull", { -6.0f, 0.4f, -3.0f }, glm::vec3(0.0f), glm::vec3(1.0f));
		m_Hull.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_HullMesh, { 0.35f, 0.45f, 0.30f, 1.0f }));
		m_Turret = MakeEntity(*m_Scene, "Turret", { 0.0f, 0.7f, 0.2f }, glm::vec3(0.0f), glm::vec3(1.0f));
		m_Turret.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_TurretMesh, { 0.42f, 0.52f, 0.36f, 1.0f }));
		m_Turret.SetParent(m_Hull, false);
		Entity barrel = MakeEntity(*m_Scene, "Barrel", { 0.0f, 0.05f, -1.4f }, { -8.0f, 0.0f, 0.0f }, glm::vec3(1.0f));
		barrel.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_BarrelMesh, { 0.25f, 0.28f, 0.24f, 1.0f }));
		barrel.SetParent(m_Turret, false);
		Entity headlight = MakeEntity(*m_Scene, "Headlight", { 0.0f, 0.0f, -0.85f }, glm::vec3(0.0f), glm::vec3(1.0f));
		SpotLightComponent spot({ 0.75f, 0.85f, 1.0f }, 4.0f, 12.0f);
		spot.InnerConeAngle = 12.0f;
		spot.OuterConeAngle = 20.0f;
		headlight.AddComponent<SpotLightComponent>(spot);
		headlight.SetParent(barrel, false);

		m_Carrier = MakeEntity(*m_Scene, "Carrier", { -5.0f, 0.0f, 5.0f }, glm::vec3(0.0f), glm::vec3(1.0f));
		Entity marker = MakeEntity(*m_Scene, "Carrier Marker", { 0.0f, 2.2f, 0.0f }, glm::vec3(0.0f), glm::vec3(0.4f));
		marker.AddComponent<MeshRendererComponent>(MeshRendererComponent(renderer.GetSphereMesh(), { 0.9f, 0.25f, 0.2f, 1.0f }));
		marker.SetParent(m_Carrier, false);
		m_Crate = MakeEntity(*m_Scene, "Crate", { 0.0f, 0.5f, 0.0f }, glm::vec3(0.0f), glm::vec3(1.0f));
		m_Crate.AddComponent<MeshRendererComponent>(MeshRendererComponent(renderer.GetBoxMesh(), { 0.62f, 0.45f, 0.25f, 1.0f }));
		m_Crate.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Dynamic));
		m_Crate.AddComponent<BoxCollider3DComponent>();
		m_Crate.SetParent(m_Carrier, false);

		m_Pivot = MakeEntity(*m_Scene, "Pivot", { 5.0f, 0.0f, 4.0f }, glm::vec3(0.0f), glm::vec3(1.0f));
		m_Paddle = MakeEntity(*m_Scene, "Paddle", { 2.2f, 0.6f, 0.0f }, glm::vec3(0.0f), { 0.3f, 1.2f, 1.4f });
		m_Paddle.AddComponent<MeshRendererComponent>(MeshRendererComponent(renderer.GetBoxMesh(), { 0.3f, 0.45f, 0.85f, 1.0f }));
		m_Paddle.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Kinematic));
		m_Paddle.AddComponent<BoxCollider3DComponent>();
		m_Paddle.SetParent(m_Pivot, false);

		Entity stand = MakeEntity(*m_Scene, "Stand", { 6.0f, 0.0f, -5.0f }, { 0.0f, 30.0f, 0.0f }, glm::vec3(2.0f));
		Entity plinth = MakeEntity(*m_Scene, "Plinth", { 0.0f, 0.5f, 0.0f }, glm::vec3(0.0f), glm::vec3(1.0f));
		plinth.AddComponent<MeshRendererComponent>(MeshRendererComponent(renderer.GetBoxMesh(), { 0.7f, 0.7f, 0.75f, 1.0f }));
		plinth.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Static));
		plinth.AddComponent<BoxCollider3DComponent>();
		plinth.SetParent(stand, false);

		m_Platform = MakeEntity(*m_Scene, "Platform", PlatformPosition(0.0f), glm::vec3(0.0f), glm::vec3(1.0f));
		m_Platform.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_PlatformMesh, { 0.55f, 0.5f, 0.35f, 1.0f }));
		m_Platform.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Kinematic));
		m_Platform.AddComponent<BoxCollider3DComponent>().HalfExtents = { 1.0f, 0.2f, 1.0f };
		m_Rider = MakeEntity(*m_Scene, "Rider", { 0.0f, 0.45f, 0.6f }, glm::vec3(0.0f), glm::vec3(0.5f));
		m_Rider.AddComponent<MeshRendererComponent>(MeshRendererComponent(renderer.GetBoxMesh(), { 0.9f, 0.6f, 0.2f, 1.0f }));
		m_Rider.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Kinematic));
		m_Rider.AddComponent<BoxCollider3DComponent>();
		m_Rider.SetParent(m_Platform, false);

		// Created deepest first, so the write-back can't rely on creation order to visit parents first.
		m_Fallers.clear();
		for (const char* name : { "Faller C", "Faller B", "Faller A" })
		{
			Entity faller = MakeEntity(*m_Scene, name, { 1.5f, 0.0f, 0.0f }, glm::vec3(0.0f), glm::vec3(1.0f));
			faller.AddComponent<MeshRendererComponent>(MeshRendererComponent(renderer.GetBoxMesh(), { 0.45f, 0.7f, 0.5f, 1.0f }));
			faller.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Dynamic));
			faller.AddComponent<BoxCollider3DComponent>();
			m_Fallers.insert(m_Fallers.begin(), faller);
		}
		Transform3DComponent& fallerRoot = m_Fallers[0].GetComponent<Transform3DComponent>();
		fallerRoot.Position = { -3.0f, 6.0f, 9.0f };
		fallerRoot.Scale = glm::vec3(0.8f);
		m_Fallers[1].SetParent(m_Fallers[0], false);
		m_Fallers[2].SetParent(m_Fallers[1], false);

		m_WalkerCarrier = MakeEntity(*m_Scene, "Walker Carrier", { 3.0f, 0.0f, 9.0f }, glm::vec3(0.0f), glm::vec3(1.0f));
		m_Walker = MakeEntity(*m_Scene, "Walker", glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f));
		m_Walker.AddComponent<CharacterController3DComponent>();
		m_Walker.SetParent(m_WalkerCarrier, false);
		Entity walkerBody = MakeEntity(*m_Scene, "Walker Body", { 0.0f, 0.9f, 0.0f }, glm::vec3(0.0f), { 0.6f, 1.8f, 0.6f });
		walkerBody.AddComponent<MeshRendererComponent>(MeshRendererComponent(renderer.GetSphereMesh(), { 0.75f, 0.4f, 0.8f, 1.0f }));
		walkerBody.SetParent(m_Walker, false);

		Entity gate = MakeEntity(*m_Scene, "Gate", { 9.0f, 0.0f, 9.0f }, glm::vec3(0.0f), glm::vec3(1.0f));
		Entity post = MakeEntity(*m_Scene, "Gate Post", { -1.0f, 1.0f, 0.0f }, glm::vec3(0.0f), { 0.4f, 2.0f, 0.4f });
		post.AddComponent<MeshRendererComponent>(MeshRendererComponent(renderer.GetBoxMesh(), { 0.6f, 0.6f, 0.65f, 1.0f }));
		post.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Static));
		post.AddComponent<BoxCollider3DComponent>();
		post.SetParent(gate, false);
		Entity bar = MakeEntity(*m_Scene, "Gate Bar", { 0.2f, 1.6f, 0.0f }, glm::vec3(0.0f), { 2.0f, 0.2f, 0.2f });
		bar.AddComponent<MeshRendererComponent>(MeshRendererComponent(renderer.GetBoxMesh(), { 0.8f, 0.2f, 0.2f, 1.0f }));
		bar.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Kinematic));
		bar.AddComponent<BoxCollider3DComponent>();
		bar.SetParent(gate, false);

		m_Scene->OnStart();

		Physics3D* physics = m_Scene->GetPhysics3D();
		RayCastHit3D hit;
		const PhysicsBodyId3D plinthBody = m_Scene->GetRuntimeBody3D(plinth);
		Check(physics && physics->RayCast(Ray({ 6.0f, 10.0f, -5.0f }, { 0.0f, -1.0f, 0.0f }), 20.0f, hit)
			&& hit.Body == plinthBody && std::abs(hit.Point.y - 2.0f) < 1e-3f && Near(physics->GetPosition(plinthBody), { 6.0f, 1.0f, -5.0f }, 1e-3f),
			"a body under a scaled parent is built at its world pose with a world-scaled collider");

		{
			Entity copy = m_Scene->DuplicateEntity(gate);
			std::vector<PhysicsBodyId3D> copyBodies;
			bool bodiesOk = physics && copy && copy.GetChildCount() == 2;
			for (Entity part : copy.GetChildren())
			{
				const PhysicsBodyId3D body = m_Scene->GetRuntimeBody3D(part);
				const Entity source = gate.FindChild(part.GetName(), false);
				bodiesOk = bodiesOk && physics->IsBodyValid(body) && body != m_Scene->GetRuntimeBody3D(source)
					&& Near(physics->GetPosition(body), part.GetWorldPosition(), 1e-4f);
				copyBodies.push_back(body);
			}
			m_Scene->DestroyEntity(copy);
			for (PhysicsBodyId3D body : copyBodies)
				bodiesOk = bodiesOk && !physics->IsBodyValid(body);
			Check(bodiesOk, "duplicating a subtree while physics runs gives every clone its own body, and destroying it frees them");
		}

		m_CrateStart = m_Crate.GetWorldPosition();
		m_CarrierStart = m_Carrier.GetWorldPosition();
		m_LastPaddlePosition = m_Paddle.GetWorldPosition();
		m_LastPlatformPosition = m_Platform.GetWorldPosition();
		m_WalkerStart = m_Walker.GetWorldPosition();

		// Thrown apart, so each faller moves relative to its parent and a child solved against its
		// parent's previous pose would land visibly off its body.
		m_Scene->SetLinearVelocity(m_Fallers[0], glm::vec3(0.0f, 4.0f, 0.0f));
		m_Scene->SetLinearVelocity(m_Fallers[2], glm::vec3(0.0f, -2.0f, 0.0f));
		m_WalkerCarrierStart = m_WalkerCarrier.GetWorldPosition();
	}

	glm::vec3 HierarchyTest::PlatformPosition(float time)
	{
		return { 2.0f + 1.5f * std::sin(1.2f * time), 0.3f, -8.0f + 1.5f * std::cos(1.2f * time) };
	}

	void HierarchyTest::RunLightProbe()
	{
		Scene probe("Light Probe");
		Entity parent = MakeEntity(probe, "Probe Parent", { 0.0f, 0.0f, -50.0f }, { 0.0f, 90.0f, 0.0f }, glm::vec3(1.0f));
		Entity child = MakeEntity(probe, "Probe Light", { 4.0f, 0.0f, 0.0f }, glm::vec3(0.0f), glm::vec3(1.0f));
		child.AddComponent<PointLightComponent>(PointLightComponent(glm::vec3(1.0f), 1.0f, 0.5f));
		child.SetParent(parent, false);

		// The child's world position is (0, 0, -54). A light placed by its local values, or by its
		// parent's translation alone, sits well outside a 10-degree cone aimed there.
		Renderer3D& renderer = Application::Get().GetRenderer3D();
		auto lightsInView = [&](const glm::vec3& target)
		{
			PerspectiveCamera camera(10.0f, 1.0f, 0.1f, 100.0f);
			camera.SetPosition({ 0.0f, 0.0f, -40.0f });
			camera.SetTarget(target);
			renderer.BeginScene(camera);
			probe.SubmitLights(renderer);
			renderer.EndScene();
			return renderer.GetStatistics().LocalLights;
		};

		const uint32_t atWorld = lightsInView({ 0.0f, 0.0f, -54.0f });
		const uint32_t atTranslationOnly = lightsInView({ 4.0f, 0.0f, -50.0f });
		Check(Near(child.GetWorldPosition(), { 0.0f, 0.0f, -54.0f }) && atWorld == 1 && atTranslationOnly == 0,
			"a point light on a child lights from the child's world position");
	}

	void HierarchyTest::Animate(float deltaTime)
	{
		if (m_Animate)
			m_Time += deltaTime;

		const float t = m_Time;
		m_Sun.GetComponent<Transform3DComponent>().Rotation = glm::angleAxis(glm::radians(25.0f * t), glm::vec3(0.0f, 1.0f, 0.0f));
		m_Planet.GetComponent<Transform3DComponent>().Rotation = glm::angleAxis(glm::radians(90.0f * t), glm::normalize(glm::vec3(0.2f, 1.0f, 0.0f)));

		Transform3DComponent& hull = m_Hull.GetComponent<Transform3DComponent>();
		const float drive = glm::radians(18.0f * t);
		hull.Position = { -6.0f + 2.0f * std::sin(drive), 0.4f, -3.0f + 2.0f * std::cos(drive) };
		hull.Rotation = glm::angleAxis(drive - glm::half_pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
		m_Turret.GetComponent<Transform3DComponent>().Rotation = glm::angleAxis(glm::radians(50.0f * std::sin(0.8f * t)), glm::vec3(0.0f, 1.0f, 0.0f));

		m_Carrier.GetComponent<Transform3DComponent>().Position.x = -5.0f + 2.5f * std::sin(1.3f * t);
		m_Pivot.GetComponent<Transform3DComponent>().Rotation = glm::angleAxis(glm::radians(45.0f * t), glm::vec3(0.0f, 1.0f, 0.0f));
		m_WalkerCarrier.GetComponent<Transform3DComponent>().Position.x = 3.0f + 2.0f * std::sin(1.1f * t);

		if (Physics3D* physics = m_Scene->GetPhysics3D())
		{
			physics->MoveKinematic(m_Scene->GetRuntimeBody3D(m_Platform), PlatformPosition(t),
				glm::angleAxis(glm::radians(60.0f * t), glm::vec3(0.0f, 1.0f, 0.0f)), deltaTime);
		}
	}

	void HierarchyTest::TrackPhysics()
	{
		Physics3D* physics = m_Scene->GetPhysics3D();
		if (!physics)
			return;

		m_MaxCrateBodyGap = (std::max)(m_MaxCrateBodyGap, glm::length(m_Crate.GetWorldPosition() - physics->GetPosition(m_Scene->GetRuntimeBody3D(m_Crate))));

		const PhysicsBodyId3D paddleBody = m_Scene->GetRuntimeBody3D(m_Paddle);
		const glm::vec3 paddlePosition = m_Paddle.GetWorldPosition();
		const float cosHalfAngle = (std::min)(std::abs(glm::dot(m_Paddle.GetWorldRotation(), physics->GetRotation(paddleBody))), 1.0f);
		m_MaxPaddleBodyGap = (std::max)(m_MaxPaddleBodyGap, glm::length(paddlePosition - physics->GetPosition(paddleBody)));
		m_MaxPaddleAngleGap = (std::max)(m_MaxPaddleAngleGap, glm::degrees(2.0f * std::acos(cosHalfAngle)));
		m_PaddleTravel += glm::length(paddlePosition - m_LastPaddlePosition);
		m_LastPaddlePosition = paddlePosition;

		const PhysicsBodyId3D riderBody = m_Scene->GetRuntimeBody3D(m_Rider);
		const float riderCosHalfAngle = (std::min)(std::abs(glm::dot(m_Rider.GetWorldRotation(), physics->GetRotation(riderBody))), 1.0f);
		m_MaxRiderGap = (std::max)(m_MaxRiderGap, glm::length(m_Rider.GetWorldPosition() - physics->GetPosition(riderBody)));
		m_MaxRiderAngleGap = (std::max)(m_MaxRiderAngleGap, glm::degrees(2.0f * std::acos(riderCosHalfAngle)));
		const glm::vec3 platformPosition = m_Platform.GetWorldPosition();
		m_PlatformTravel += glm::length(platformPosition - m_LastPlatformPosition);
		m_LastPlatformPosition = platformPosition;

		for (Entity faller : m_Fallers)
			m_MaxFallerGap = (std::max)(m_MaxFallerGap, glm::length(faller.GetWorldPosition() - physics->GetPosition(m_Scene->GetRuntimeBody3D(faller))));

		if (CharacterController3D* walker = m_Scene->GetCharacterController(m_Walker))
			m_MaxWalkerGap = (std::max)(m_MaxWalkerGap, glm::length(m_Walker.GetWorldPosition() - walker->GetPosition()));
		else
			m_MaxWalkerGap = 1.0f;
		m_WalkerCarrierTravel = (std::max)(m_WalkerCarrierTravel, glm::length(m_WalkerCarrier.GetWorldPosition() - m_WalkerCarrierStart));
	}

	void HierarchyTest::RunPhysicsChecks()
	{
		const glm::vec3 crate = m_Crate.GetWorldPosition();
		const float crateDrift = glm::length(glm::vec2(crate.x - m_CrateStart.x, crate.z - m_CrateStart.z));
		const float carrierTravel = std::abs(m_Carrier.GetWorldPosition().x - m_CarrierStart.x);
		Check(m_MaxCrateBodyGap < 1e-3f && crateDrift < 0.05f && carrierTravel > 1.0f && std::abs(crate.y - 0.5f) < 0.05f,
			std::format("a dynamic body under a moving parent stays where physics puts it (gap {:.1e}, drift {:.3f}, parent moved {:.2f})",
				m_MaxCrateBodyGap, crateDrift, carrierTravel));
		Check(m_MaxPaddleBodyGap < 1e-3f && m_MaxPaddleAngleGap < 0.1f && m_PaddleTravel > 1.0f,
			std::format("a kinematic child follows its parent (gap {:.1e}, {:.3f} deg over {:.1f} units)", m_MaxPaddleBodyGap, m_MaxPaddleAngleGap, m_PaddleTravel));
		Check(m_MaxRiderGap < 1e-3f && m_MaxRiderAngleGap < 0.1f && m_PlatformTravel > 1.0f,
			std::format("a kinematic child of a MoveKinematic'd parent keeps up within the step (gap {:.1e}, {:.3f} deg; the parent travelled {:.1f})",
				m_MaxRiderGap, m_MaxRiderAngleGap, m_PlatformTravel));

		float lowest = 1e9f;
		for (Entity faller : m_Fallers)
			lowest = (std::min)(lowest, faller.GetWorldPosition().y);
		Check(m_MaxFallerGap < 1e-3f && m_Fallers.size() == 3 && lowest < 1.0f,
			std::format("nested dynamic bodies are written back parents first (gap {:.1e} while falling)", m_MaxFallerGap));

		const glm::vec3 walker = m_Walker.GetWorldPosition();
		const float walkerDrift = glm::length(glm::vec2(walker.x - m_WalkerStart.x, walker.z - m_WalkerStart.z));
		Check(m_MaxWalkerGap < 1e-3f && walkerDrift < 0.05f && m_WalkerCarrierTravel > 1.0f,
			std::format("a character controller under a moving parent stays where it walked (gap {:.1e}, drift {:.3f}, parent moved up to {:.2f})",
				m_MaxWalkerGap, walkerDrift, m_WalkerCarrierTravel));
	}

	void HierarchyTest::Update(float deltaTime)
	{
		const float step = (std::min)(deltaTime, 1.0f / 30.0f);

		if (!m_LightProbeDone && !Renderer::IsFrameSkipped())
		{
			m_LightProbeDone = true;
			RunLightProbe();
		}

		if (m_StressScene)
		{
			UpdateStress(deltaTime);
			return;
		}

		Animate(step);
		Animate2D(step);
		m_Scene->OnUpdate(step);
		m_Scene2D->OnUpdate(step);

		if (!m_PhysicsChecksDone)
		{
			TrackPhysics();
			TrackPhysics2D();
			if (m_Time >= k_PhysicsCheckSeconds)
			{
				m_PhysicsChecksDone = true;
				RunPhysicsChecks();
				RunPhysicsChecks2D();
			}
		}

		if (m_View != View::Scene3D)
		{
			Render2D();
			return;
		}

		if (m_AutoOrbit)
			m_OrbitAngle = std::fmod(m_OrbitAngle + step * 8.0f, 360.0f);
		const float orbit = glm::radians(m_OrbitAngle);
		m_Camera.SetPosition({ std::sin(orbit) * 19.0f, 10.0f, std::cos(orbit) * 19.0f });
		m_Camera.SetTarget({ 0.0f, 1.0f, 0.0f });

		Renderer3D& renderer = Application::Get().GetRenderer3D();
		renderer.BeginScene(m_Camera);
		renderer.Clear(m_ClearColor);
		m_Scene->SubmitLights(renderer);
		m_Scene->RenderEntities3D(renderer);
		renderer.EndScene();
	}

	void HierarchyTest::Cleanup()
	{
		delete m_Scene;
		m_Scene = nullptr;
		delete m_Scene2D;
		m_Scene2D = nullptr;
		DestroyAndDelete(m_Font);
		m_Fallers2D.clear();
		delete m_StressScene;
		m_StressScene = nullptr;
		m_StressSpinners.clear();
		m_StressTime = 0.0f;
		m_StressFrames = 0;
		m_StressFrameMs = m_StressUpdateMs = m_StressRenderMs = m_StressEndSceneMs = 0.0;
		m_StressDroppedMeshes = 0;
		m_StressResult.clear();

		delete m_HullMesh;
		delete m_TurretMesh;
		delete m_BarrelMesh;
		delete m_PlatformMesh;
		m_HullMesh = nullptr;
		m_TurretMesh = nullptr;
		m_BarrelMesh = nullptr;
		m_PlatformMesh = nullptr;
		m_Fallers.clear();
	}

	void HierarchyTest::Resize(uint32_t width, uint32_t height)
	{
		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
		m_Camera.SetAspectRatio(m_AspectRatio);
	}

	void HierarchyTest::ImGuiRender()
	{
		GraphicsTest::ImGuiRender();
		ImGui::Separator();

		if (ImGui::Button("Reset"))
		{
			Cleanup();
			Initialize();
			return;
		}
		ImGui::SameLine();
		ImGui::Checkbox("Animate", &m_Animate);
		ImGui::SameLine();
		ImGui::Checkbox("Auto Orbit", &m_AutoOrbit);
		if (!m_AutoOrbit)
			ImGui::SliderFloat("Orbit", &m_OrbitAngle, 0.0f, 360.0f);

		if (m_StressScene)
		{
			ImGui::TextWrapped("Stress: %s", m_StressResult.empty() ? "measuring..." : m_StressResult.c_str());
		}
		else
		{
			int view = static_cast<int>(m_View);
			ImGui::RadioButton("3D", &view, static_cast<int>(View::Scene3D));
			ImGui::SameLine();
			ImGui::RadioButton("2D", &view, static_cast<int>(View::Scene2D));
			ImGui::SameLine();
			ImGui::RadioButton("2D probe", &view, static_cast<int>(View::Probe2D));
			m_View = static_cast<View>(view);
		}
		if (m_View == View::Probe2D)
			ImGui::TextWrapped("The magenta muzzle must sit in the centre: the view is placed by the test's own arithmetic.");

		ImGui::Separator();
		for (const CheckResult& check : m_Checks)
		{
			const ImVec4 color = check.Passed ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f) : ImVec4(0.95f, 0.3f, 0.3f, 1.0f);
			ImGui::TextColored(color, "%s %s", check.Passed ? "[PASS]" : "[FAIL]", check.Name.c_str());
		}
		if (!m_PhysicsChecksDone && !m_StressScene)
			ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.3f, 1.0f), "[....] physics checks run after %.0f s of animation", k_PhysicsCheckSeconds);
	}

}
