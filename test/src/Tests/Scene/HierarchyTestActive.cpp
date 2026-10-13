#include "HierarchyTest.h"

#include <cmath>
#include <format>

namespace
{
	using namespace Dingo;

	struct ScriptLog
	{
		int Starts = 0;
		int Updates = 0;
		int FixedUpdates = 0;
		std::string Order;
		float LastStep = 0.0f;
	};

	class CountingScript : public ScriptableEntity
	{
	public:
		explicit CountingScript(ScriptLog* log) : m_Log(log) {}

	protected:
		void OnStart() override { m_Log->Starts++; }
		void OnUpdate(float) override { m_Log->Updates++; m_Log->Order += 'U'; }
		void OnFixedUpdate(float step) override { m_Log->FixedUpdates++; m_Log->Order += 'F'; m_Log->LastStep = step; }

	private:
		ScriptLog* m_Log;
	};

	Entity Make3D(Scene& scene, const std::string& name, const glm::vec3& position)
	{
		Entity entity = scene.CreateEntity(name);
		entity.AddComponent<Transform3DComponent>(Transform3DComponent(position));
		return entity;
	}
}

namespace Dingo
{

	void HierarchyTest::RunActiveChecks()
	{
		{
			Scene scene("Active Structure");
			Entity root = Make3D(scene, "Root", glm::vec3(0.0f));
			Entity child = Make3D(scene, "Child", glm::vec3(0.0f));
			Entity grandchild = Make3D(scene, "Grandchild", glm::vec3(0.0f));
			child.SetParent(root);
			grandchild.SetParent(child);

			Check(root.IsActive() && child.IsActiveInHierarchy() && grandchild.IsActiveInHierarchy(), "a new entity is active");
			child.SetActive(false);
			Check(!child.IsActive() && !child.IsActiveInHierarchy() && root.IsActiveInHierarchy(), "SetActive(false) clears the entity's own flag and leaves its parent active");
			Check(grandchild.IsActive() && !grandchild.IsActiveInHierarchy(), "a child of an inactive entity keeps its own flag but is inactive in the hierarchy");

			grandchild.SetParent(root);
			Check(grandchild.IsActiveInHierarchy(), "reparenting out from under an inactive entity activates the subtree");
			grandchild.SetParent(child);
			Check(!grandchild.IsActiveInHierarchy(), "reparenting under an inactive entity deactivates the subtree");

			Entity copy = scene.DuplicateEntity(root);
			Entity copiedChild = copy.FindChild("Child");
			Entity copiedGrandchild = copy.FindChild("Grandchild");
			Check(copy.IsActiveInHierarchy() && copiedChild && !copiedChild.IsActive() && copiedGrandchild && copiedGrandchild.IsActive() && !copiedGrandchild.IsActiveInHierarchy(),
				"a duplicate keeps each entity's own flag, and its inactive subtree stays inactive");

			child.SetActive(true);
			Check(child.IsActiveInHierarchy() && grandchild.IsActiveInHierarchy(), "SetActive(true) activates the subtree again");

			root.SetActive(false);
			grandchild.RemoveParent();
			Check(grandchild.IsActiveInHierarchy() && !child.IsActiveInHierarchy(), "RemoveParent from an inactive branch activates the entity");
		}

		{
			Scene scene("Active Scripts");
			ScriptLog sleeper, worker;
			Entity sleeping = scene.CreateEntity("Sleeper");
			sleeping.AddScript<CountingScript>(&sleeper);
			sleeping.SetActive(false);
			Entity working = scene.CreateEntity("Worker");
			working.AddScript<CountingScript>(&worker);

			scene.OnStart();
			for (int i = 0; i < 10; i++)
				scene.OnUpdate(1.0f / 60.0f);
			Check(sleeper.Starts == 0 && sleeper.Updates == 0 && sleeper.FixedUpdates == 0,
				std::format("an inactive entity's script neither starts nor updates ({} starts, {} updates, {} fixed)", sleeper.Starts, sleeper.Updates, sleeper.FixedUpdates));
			Check(worker.Starts == 1 && worker.Updates == 10 && worker.FixedUpdates == 10,
				std::format("an active script at 60 Hz gets one OnFixedUpdate per OnUpdate ({} updates, {} fixed)", worker.Updates, worker.FixedUpdates));

			sleeping.SetActive(true);
			scene.OnUpdate(1.0f / 60.0f);
			Check(sleeper.Starts == 1 && sleeper.Updates == 1, "activating an entity starts its script before its first OnUpdate");

			working.SetActive(false);
			const int updates = worker.Updates;
			scene.OnUpdate(1.0f / 60.0f);
			Check(worker.Updates == updates, "a deactivated script stops updating");

			worker.Order.clear();
			working.SetActive(true);
			scene.OnUpdate(1.0f / 30.0f);
			Check(worker.Order == "FFU", std::format("a 1/30 s update runs two fixed steps before OnUpdate ('{}')", worker.Order));

			worker.FixedUpdates = 0;
			scene.OnUpdate(1.0f / 120.0f);
			scene.OnUpdate(1.0f / 120.0f);
			Check(worker.FixedUpdates == 1, std::format("two 1/120 s updates carry over into one fixed step ({})", worker.FixedUpdates));

			scene.SetFixedTimeStep(0.001f);
			worker.FixedUpdates = 0;
			scene.OnUpdate(1.0f);
			Check(worker.FixedUpdates == static_cast<int>(Scene::k_MaxFixedStepsPerUpdate) && scene.GetFixedTimeAlpha() < 1.0f,
				std::format("a stall runs at most {} fixed steps and drops the rest ({}, alpha {:.2f})", Scene::k_MaxFixedStepsPerUpdate, worker.FixedUpdates, scene.GetFixedTimeAlpha()));

			scene.SetFixedTimeStep(0.02f);
			worker.FixedUpdates = 0;
			scene.OnUpdate(0.05f);
			Check(worker.FixedUpdates == 2 && std::abs(worker.LastStep - 0.02f) < 1e-6f && scene.GetFixedTimeAlpha() >= 0.49f && scene.GetFixedTimeAlpha() < 0.56f,
				std::format("OnFixedUpdate runs at the set step, the remainder carried as alpha ({} steps of {:.3f}, alpha {:.2f})", worker.FixedUpdates, worker.LastStep, scene.GetFixedTimeAlpha()));
		}

		{
			Scene scene("Active Physics");
			Entity floor = Make3D(scene, "Floor", { 0.0f, -0.5f, 0.0f });
			floor.GetComponent<Transform3DComponent>().Scale = { 20.0f, 1.0f, 20.0f };
			floor.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Static));
			floor.AddComponent<BoxCollider3DComponent>();

			Entity carrier = Make3D(scene, "Carrier", { 0.0f, 5.0f, 0.0f });
			Entity box = Make3D(scene, "Box", glm::vec3(0.0f));
			box.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Dynamic));
			box.AddComponent<BoxCollider3DComponent>();
			box.SetParent(carrier, false);
			Entity walker = Make3D(scene, "Walker", { 4.0f, 2.0f, 0.0f });
			walker.AddComponent<CharacterController3DComponent>();
			walker.SetParent(carrier);

			carrier.SetActive(false);
			scene.OnStart();
			Check(scene.GetPhysics3D() && scene.GetRuntimeBody3D(box) == k_InvalidBody3D && !scene.GetCharacterController(walker),
				"an entity inactive at start gets no body or controller");

			for (int i = 0; i < 30; i++)
				scene.OnUpdate(1.0f / 60.0f);
			Check(std::abs(box.GetWorldPosition().y - 5.0f) < 1e-5f, "an inactive body doesn't fall");

			carrier.SetActive(true);
			const PhysicsBodyId3D body = scene.GetRuntimeBody3D(box);
			Check(body != k_InvalidBody3D && scene.GetCharacterController(walker) && scene.GetEntityFromBody3D(body) == box,
				"activating the parent builds its children's body and controller");

			for (int i = 0; i < 30; i++)
				scene.OnUpdate(1.0f / 60.0f);
			const float fallen = box.GetWorldPosition().y;
			Check(fallen < 4.5f, std::format("the rebuilt body falls (y {:.2f})", fallen));

			carrier.SetActive(false);
			Check(scene.GetRuntimeBody3D(box) == k_InvalidBody3D && !scene.GetCharacterController(walker) && !scene.GetEntityFromBody3D(body),
				"deactivating the parent removes its children's body and controller");
			for (int i = 0; i < 30; i++)
				scene.OnUpdate(1.0f / 60.0f);
			Check(std::abs(box.GetWorldPosition().y - fallen) < 1e-5f, "the transform stays where the body was");

			box.SetActive(false);
			carrier.SetActive(true);
			Check(scene.GetRuntimeBody3D(box) == k_InvalidBody3D && scene.GetCharacterController(walker), "a child with its own flag off stays without a body when the parent comes back");
		}

		{
			Scene scene("Active Physics 2D");
			Entity crate = scene.CreateEntity("Crate");
			crate.AddComponent<RigidBody2DComponent>().Type = BodyType2D::Dynamic;
			crate.AddComponent<BoxCollider2DComponent>();
			scene.OnStart();
			const bool built = scene.GetRuntimeBody2D(crate) != 0;
			crate.SetActive(false);
			const bool removed = scene.GetRuntimeBody2D(crate) == 0;
			crate.SetActive(true);
			Check(built && removed && scene.GetRuntimeBody2D(crate) != 0, "a 2D body is removed and rebuilt with its entity");
		}

		{
			Scene scene("Active Cameras And Lights");
			Entity main = Make3D(scene, "Main", glm::vec3(0.0f));
			main.AddComponent<CameraComponent>().Type = CameraComponent::ProjectionType::Perspective;
			Entity backup = Make3D(scene, "Backup", glm::vec3(0.0f));
			backup.AddComponent<CameraComponent>().Primary = false;
			Entity lamp = Make3D(scene, "Lamp", glm::vec3(0.0f));
			lamp.AddComponent<PointLightComponent>();

			main.SetActive(false);
			Entity camera;
			Check(scene.GetPrimaryCameraEntity(camera) && camera == backup, "an inactive camera is never the primary one");

			const float lit = scene.GetShadowedLightAttenuation(lamp, { 1.0f, 0.0f, 0.0f });
			lamp.SetActive(false);
			Check(lit > 0.0f && scene.GetShadowedLightAttenuation(lamp, { 1.0f, 0.0f, 0.0f }) == 0.0f, "an inactive light lights nothing");
		}
	}

}
