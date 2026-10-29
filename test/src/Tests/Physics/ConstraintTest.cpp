#include "ConstraintTest.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <memory>

namespace
{
	using namespace Dingo;

	constexpr float k_Dt = 1.0f / 120.0f;

	std::unique_ptr<Physics3D> MakeWorld(const glm::vec3& gravity)
	{
		std::unique_ptr<Physics3D> world(Physics3D::Create());
		Physics3DParams params;
		params.Gravity = gravity;
		world->Initialize(params);
		return world;
	}

	PhysicsBodyId3D Box(Physics3D& world, BodyType3D type, const glm::vec3& position, const glm::vec3& halfExtents = glm::vec3(0.5f))
	{
		RigidBodyParams3D params;
		params.Type = type;
		params.Shape = ColliderShape3D::Box;
		params.Position = position;
		params.HalfExtents = halfExtents;
		return world.CreateBody(params);
	}

	void Run(Physics3D& world, float seconds)
	{
		const int steps = static_cast<int>(seconds / k_Dt);
		for (int i = 0; i < steps; i++)
			world.Step(k_Dt, 1);
	}

	float Distance(const glm::vec3& a, const glm::vec3& b) { return glm::length(a - b); }

	const MotorParams3D k_Hold{ 100000.0f, 10000.0f, 100000.0f };
	const glm::vec3 k_AxisZ{ 0.0f, 0.0f, 1.0f };

	struct Arm
	{
		std::unique_ptr<Physics3D> World;
		PhysicsBodyId3D Anchor = k_InvalidBody3D;
		PhysicsBodyId3D Body = k_InvalidBody3D;
		ConstraintId3D Joint = k_InvalidConstraint3D;
	};

	Arm MakeArm(const glm::vec3& gravity, float minLimit = 0.0f, float maxLimit = 0.0f)
	{
		Arm arm;
		arm.World = MakeWorld(gravity);
		arm.Anchor = Box(*arm.World, BodyType3D::Static, glm::vec3(0.0f), glm::vec3(0.25f));
		arm.Body = Box(*arm.World, BodyType3D::Dynamic, { 2.0f, 0.0f, 0.0f }, glm::vec3(0.25f));
		ConstraintParams3D params(ConstraintType3D::Hinge, arm.Anchor, arm.Body);
		params.AnchorB = { -2.0f, 0.0f, 0.0f };
		params.AxisA = params.AxisB = k_AxisZ;
		params.MinLimit = minLimit;
		params.MaxLimit = maxLimit;
		arm.Joint = arm.World->CreateConstraint(params);
		return arm;
	}

	float Angle(const Arm& arm) { return arm.World->GetConstraintValue(arm.Joint); }

	glm::quat AboutZ(float angle) { return glm::angleAxis(angle, k_AxisZ); }

	float AngleOf(const glm::quat& rotation) { return 2.0f * std::atan2(rotation.z, rotation.w); }
}

namespace Dingo
{

	void ConstraintTest::RunTypeChecks()
	{
		{
			auto world = MakeWorld({ 0.0f, -9.81f, 0.0f });
			const PhysicsBodyId3D body = Box(*world, BodyType3D::Dynamic, { 0.0f, 5.0f, 0.0f });
			ConstraintParams3D params(ConstraintType3D::Fixed, body, k_InvalidBody3D);
			params.AnchorB = { 0.0f, 5.0f, 0.0f };
			params.UserData = 77;
			const ConstraintId3D id = world->CreateConstraint(params);
			Check(world->IsConstraintValid(id), "fixed: pin to the world is created");
			Check(world->GetConstraintUserData(id) == 77, "fixed: user data round-trips");
			Check(world->GetConstraintBody(id, 0) == body && world->GetConstraintBody(id, 1) == k_InvalidBody3D,
				"fixed: GetConstraintBody reports the body and none for the world");
			world->SetAngularVelocity(body, { 1.0f, 2.0f, 3.0f });
			Run(*world, 1.0f);
			Check(Distance(world->GetPosition(body), { 0.0f, 5.0f, 0.0f }) < 0.03f, "fixed: a world-pinned body hangs in place under gravity");
			Check(std::abs(glm::dot(world->GetRotation(body) * glm::vec3(0, 1, 0), glm::vec3(0, 1, 0))) > 0.99f, "fixed: a world-pinned body does not rotate");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			const PhysicsBodyId3D a = Box(*world, BodyType3D::Dynamic, { 0.0f, 0.0f, 0.0f });
			const PhysicsBodyId3D b = Box(*world, BodyType3D::Dynamic, { 3.0f, 0.0f, 0.0f });
			ConstraintParams3D params(ConstraintType3D::Fixed, a, b);
			params.AnchorA = { 1.5f, 0.0f, 0.0f };
			params.AnchorB = { -1.5f, 0.0f, 0.0f };
			const ConstraintId3D id = world->CreateConstraint(params);
			world->SetAngularVelocity(a, { 0.0f, 0.0f, 2.0f });
			world->SetLinearVelocity(b, { 0.0f, 1.0f, 0.0f });
			Run(*world, 1.5f);
			const glm::vec3 xa = world->GetRotation(a) * glm::vec3(1, 0, 0);
			const glm::vec3 xb = world->GetRotation(b) * glm::vec3(1, 0, 0);
			Check(world->IsConstraintValid(id) && std::abs(Distance(world->GetPosition(a), world->GetPosition(b)) - 3.0f) < 0.05f,
				"fixed: two dynamic bodies keep their distance");
			Check(glm::dot(xa, xb) > 0.99f, "fixed: two dynamic bodies keep their relative orientation");
		}
		{
			auto world = MakeWorld({ 0.0f, -9.81f, 0.0f });
			const PhysicsBodyId3D body = Box(*world, BodyType3D::Dynamic, { 2.0f, 0.0f, 0.0f });
			ConstraintParams3D params(ConstraintType3D::Point, body, k_InvalidBody3D);
			params.AnchorA = { -2.0f, 0.0f, 0.0f };
			const ConstraintId3D id = world->CreateConstraint(params);
			float worstError = 0.0f;
			for (int i = 0; i < 240; i++)
			{
				world->Step(k_Dt, 1);
				const glm::vec3 pin = world->GetPosition(body) + world->GetRotation(body) * glm::vec3(-2.0f, 0.0f, 0.0f);
				worstError = (std::max)(worstError, glm::length(pin));
			}
			Check(world->IsConstraintValid(id) && worstError < 0.05f, "point: the pendulum's pin stays at the world anchor");
			Check(world->GetPosition(body).y < -0.5f, "point: the body swings freely about the pin");
		}
		{
			auto world = MakeWorld({ 0.0f, -9.81f, 0.0f });
			const PhysicsBodyId3D body = Box(*world, BodyType3D::Dynamic, { 2.0f, 0.0f, 0.0f });
			ConstraintParams3D params(ConstraintType3D::Hinge, body, k_InvalidBody3D);
			params.AnchorA = { -2.0f, 0.0f, 0.0f };
			const ConstraintId3D id = world->CreateConstraint(params);
			world->SetLinearVelocity(body, { 0.0f, 0.0f, 3.0f });
			float worstY = 0.0f;
			float minAngle = 0.0f, maxAngle = 0.0f;
			for (int i = 0; i < 240; i++)
			{
				world->Step(k_Dt, 1);
				worstY = (std::max)(worstY, std::abs(world->GetPosition(body).y));
				const float angle = world->GetConstraintValue(id);
				minAngle = (std::min)(minAngle, angle);
				maxAngle = (std::max)(maxAngle, angle);
			}
			Check(worstY < 0.05f, "hinge: a body on a vertical axis ignores gravity and stays in its plane");
			Check(maxAngle - minAngle > 0.5f, "hinge: GetConstraintValue reports the angle as it rotates");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			const PhysicsBodyId3D body = Box(*world, BodyType3D::Dynamic, glm::vec3(0.0f));
			ConstraintParams3D params(ConstraintType3D::Hinge, body, k_InvalidBody3D);
			const ConstraintId3D id = world->CreateConstraint(params);
			world->SetAngularVelocity(body, { 3.0f, 3.0f, 0.0f });
			Run(*world, 1.0f);
			Check(world->IsConstraintValid(id) && glm::dot(world->GetRotation(body) * glm::vec3(0, 1, 0), glm::vec3(0, 1, 0)) > 0.99f,
				"hinge: rotation about any other axis is resisted");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			const PhysicsBodyId3D body = Box(*world, BodyType3D::Dynamic, glm::vec3(0.0f));
			ConstraintParams3D params(ConstraintType3D::Hinge, body, k_InvalidBody3D);
			params.MinLimit = -0.5f;
			params.MaxLimit = 0.5f;
			const ConstraintId3D id = world->CreateConstraint(params);
			world->SetAngularVelocity(body, { 0.0f, 5.0f, 0.0f });
			float hi = 0.0f;
			for (int i = 0; i < 240; i++)
			{
				world->Step(k_Dt, 1);
				hi = (std::max)(hi, std::abs(world->GetConstraintValue(id)));
			}
			Check(world->IsConstraintValid(id) && hi < 0.56f && hi > 0.4f, "hinge: stops at its limit and reaches it");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			const PhysicsBodyId3D body = Box(*world, BodyType3D::Dynamic, glm::vec3(0.0f));
			ConstraintParams3D params(ConstraintType3D::SwingTwist, body, k_InvalidBody3D);
			params.SwingLimitY = 0.3f;
			params.SwingLimitZ = 0.3f;
			params.TwistMin = -0.2f;
			params.TwistMax = 0.2f;
			const ConstraintId3D id = world->CreateConstraint(params);
			world->SetAngularVelocity(body, { 4.0f, 4.0f, 4.0f });
			float worstSwing = 0.0f, worstTwist = 0.0f;
			for (int i = 0; i < 240; i++)
			{
				world->Step(k_Dt, 1);
				const glm::quat q = world->GetRotation(body);
				const glm::vec3 up = q * glm::vec3(0, 1, 0);
				worstSwing = (std::max)(worstSwing, std::acos((std::min)(1.0f, up.y)));
				const float twist = 2.0f * std::atan2(q.y, q.w);
				worstTwist = (std::max)(worstTwist, std::abs(std::remainder(twist, glm::two_pi<float>())));
			}
			Check(world->IsConstraintValid(id) && worstSwing < 0.36f, "swing-twist: the swing stays inside its cone");
			Check(worstTwist < 0.26f, "swing-twist: the twist stays inside its range");
		}
		{
			auto world = MakeWorld({ 0.0f, -9.81f, 0.0f });
			const PhysicsBodyId3D anchor = Box(*world, BodyType3D::Static, { 0.0f, 10.0f, 0.0f });
			const PhysicsBodyId3D body = Box(*world, BodyType3D::Dynamic, { 0.0f, 8.5f, 0.0f });
			ConstraintParams3D params(ConstraintType3D::Distance, body, anchor);
			params.MinLimit = 1.0f;
			params.MaxLimit = 2.0f;
			const ConstraintId3D id = world->CreateConstraint(params);
			Run(*world, 2.0f);
			const float hanging = Distance(world->GetPosition(body), world->GetPosition(anchor));
			Check(world->IsConstraintValid(id) && hanging > 1.95f && hanging < 2.05f, "distance: a body falls until the maximum distance holds it");
			world->SetGravity(glm::vec3(0.0f));
			world->SetLinearVelocity(body, { 0.0f, 6.0f, 0.0f });
			float closest = 100.0f;
			for (int i = 0; i < 120; i++)
			{
				world->Step(k_Dt, 1);
				closest = (std::min)(closest, Distance(world->GetPosition(body), world->GetPosition(anchor)));
			}
			Check(closest > 0.95f, "distance: the minimum distance stops a body that closes in");
		}
		{
			auto world = MakeWorld({ 0.0f, -9.81f, 0.0f });
			const PhysicsBodyId3D body = Box(*world, BodyType3D::Dynamic, { 1.5f, 0.0f, 0.0f });
			ConstraintParams3D params(ConstraintType3D::Distance, body, k_InvalidBody3D);
			params.MinLimit = 1.5f;
			params.MaxLimit = 1.5f;
			const ConstraintId3D id = world->CreateConstraint(params);
			float worst = 0.0f;
			for (int i = 0; i < 240; i++)
			{
				world->Step(k_Dt, 1);
				worst = (std::max)(worst, std::abs(glm::length(world->GetPosition(body)) - 1.5f));
			}
			Check(world->IsConstraintValid(id) && worst < 0.05f, "distance: Min == Max is a rigid rod");
		}
		{
			auto world = MakeWorld({ 0.0f, -9.81f, 0.0f });
			const PhysicsBodyId3D body = Box(*world, BodyType3D::Dynamic, glm::vec3(0.0f));
			ConstraintParams3D params(ConstraintType3D::Slider, body, k_InvalidBody3D);
			params.AxisA = { 1.0f, 0.0f, 0.0f };
			params.AxisB = { 1.0f, 0.0f, 0.0f };
			params.NormalAxisA = { 0.0f, 1.0f, 0.0f };
			params.NormalAxisB = { 0.0f, 1.0f, 0.0f };
			params.MinLimit = -1.0f;
			params.MaxLimit = 1.0f;
			const ConstraintId3D id = world->CreateConstraint(params);
			world->SetLinearVelocity(body, { 2.0f, 2.0f, 2.0f });
			float farthest = 0.0f, offAxis = 0.0f;
			for (int i = 0; i < 240; i++)
			{
				world->Step(k_Dt, 1);
				const glm::vec3 p = world->GetPosition(body);
				farthest = (std::max)(farthest, p.x);
				offAxis = (std::max)(offAxis, (std::max)(std::abs(p.y), std::abs(p.z)));
			}
			Check(world->IsConstraintValid(id) && offAxis < 0.03f, "slider: moves along its axis only, gravity and sideways motion are held");
			Check(farthest > 0.9f && farthest < 1.06f, "slider: stops at its limit");
			Check(std::abs(std::abs(world->GetConstraintValue(id)) - 1.0f) < 0.06f, "slider: GetConstraintValue reports the displacement");
		}
	}

	void ConstraintTest::RunPairChecks()
	{
		for (const bool collide : { false, true })
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			const PhysicsBodyId3D a = Box(*world, BodyType3D::Dynamic, glm::vec3(0.0f));
			const PhysicsBodyId3D b = Box(*world, BodyType3D::Dynamic, { 0.5f, 0.0f, 0.0f });
			ConstraintParams3D params(ConstraintType3D::Distance, a, b);
			params.MaxLimit = 5.0f;
			params.CollideConnected = collide;
			const ConstraintId3D id = world->CreateConstraint(params);
			Run(*world, 1.0f);
			const float apart = Distance(world->GetPosition(a), world->GetPosition(b));
			Check(world->IsConstraintValid(id), collide ? "collide-connected: constraint created" : "no-collide: constraint created");
			if (collide)
				Check(apart > 0.95f, "CollideConnected true: overlapping joined bodies push apart");
			else
				Check(apart < 0.6f, "CollideConnected false: joined bodies pass through each other");
			Check(world->IsCollisionIgnored(a, b) == !collide, collide ? "CollideConnected true: no pair ignore" : "CollideConnected false: the pair is ignored");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			const PhysicsBodyId3D a = Box(*world, BodyType3D::Dynamic, glm::vec3(0.0f));
			const PhysicsBodyId3D b = Box(*world, BodyType3D::Dynamic, { 0.5f, 0.0f, 0.0f });
			const PhysicsBodyId3D c = Box(*world, BodyType3D::Dynamic, { 0.0f, 0.6f, 0.0f });
			ConstraintParams3D params(ConstraintType3D::Distance, a, b);
			params.MaxLimit = 5.0f;
			world->CreateConstraint(params);
			Run(*world, 1.0f);
			Check(Distance(world->GetPosition(a), world->GetPosition(c)) > 0.95f, "no other pair is affected: a third body still collides with a joined one");
			Check(!world->IsCollisionIgnored(a, c) && !world->IsCollisionIgnored(b, c), "no other pair is affected: IsCollisionIgnored");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			const PhysicsBodyId3D a = Box(*world, BodyType3D::Dynamic, glm::vec3(0.0f));
			const PhysicsBodyId3D b = Box(*world, BodyType3D::Dynamic, { 0.5f, 0.0f, 0.0f });
			ConstraintParams3D params(ConstraintType3D::Point, a, b);
			world->CreateConstraint(params);
			std::vector<PhysicsBodyId3D> hits;
			world->OverlapSphere({ 0.25f, 0.0f, 0.0f }, 0.1f, hits);
			const bool both = std::find(hits.begin(), hits.end(), a) != hits.end() && std::find(hits.begin(), hits.end(), b) != hits.end();
			Check(both, "queries still report both joined bodies");
			RayCastHit3D hit;
			Check(world->RayCast(Ray({ -3.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }), 10.0f, hit) && hit.Body == a, "ray casts still hit a joined body");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			const PhysicsBodyId3D a = Box(*world, BodyType3D::Dynamic, glm::vec3(0.0f));
			const PhysicsBodyId3D b = Box(*world, BodyType3D::Dynamic, { 3.0f, 0.0f, 0.0f });
			world->IgnoreCollision(a, b, true);
			ConstraintParams3D params(ConstraintType3D::Distance, a, b);
			params.MinLimit = 3.0f;
			params.MaxLimit = 3.0f;
			const ConstraintId3D id = world->CreateConstraint(params);
			world->IgnoreCollision(a, b, false);
			Check(world->IsCollisionIgnored(a, b), "a constraint's ignore survives IgnoreCollision(false)");
			world->DestroyConstraint(id);
			Check(!world->IsCollisionIgnored(a, b), "destroying the constraint and clearing the user ignore frees the pair");
			world->IgnoreCollision(a, b, true);
			const ConstraintId3D second = world->CreateConstraint(params);
			world->DestroyConstraint(second);
			Check(world->IsCollisionIgnored(a, b), "destroying the constraint keeps the user's ignore");
			world->IgnoreCollision(a, b, false);
			Check(!world->IsCollisionIgnored(a, b), "the user ignore clears once the constraint is gone");
		}
		{
			auto world = MakeWorld({ 0.0f, -9.81f, 0.0f });
			const PhysicsBodyId3D body = Box(*world, BodyType3D::Dynamic, { 0.0f, 5.0f, 0.0f });
			const PhysicsBodyId3D other = Box(*world, BodyType3D::Static, { 0.0f, 5.0f, 0.0f }, { 0.1f, 0.1f, 0.1f });
			ConstraintParams3D params(ConstraintType3D::Fixed, body, other);
			const ConstraintId3D id = world->CreateConstraint(params);
			Check(world->IsConstraintEnabled(id), "a new constraint is enabled");
			world->SetConstraintEnabled(id, false);
			Check(!world->IsConstraintEnabled(id) && world->IsConstraintValid(id), "SetConstraintEnabled(false) disables but keeps the handle");
			Check(world->IsCollisionIgnored(body, other), "a disabled constraint keeps its pair ignore");
			Run(*world, 0.5f);
			Check(world->GetPosition(body).y < 4.5f, "a disabled constraint lets the body fall");
			world->SetConstraintEnabled(id, true);
			Check(world->IsConstraintEnabled(id), "SetConstraintEnabled(true) re-enables");
		}
	}

	void ConstraintTest::RunLifetimeChecks()
	{
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			const PhysicsBodyId3D a = Box(*world, BodyType3D::Dynamic, glm::vec3(0.0f));
			const PhysicsBodyId3D b = Box(*world, BodyType3D::Dynamic, { 3.0f, 0.0f, 0.0f });
			const PhysicsBodyId3D c = Box(*world, BodyType3D::Dynamic, { 6.0f, 0.0f, 0.0f });
			const ConstraintId3D ab = world->CreateConstraint(ConstraintParams3D(ConstraintType3D::Point, a, b));
			const ConstraintId3D bc = world->CreateConstraint(ConstraintParams3D(ConstraintType3D::Point, b, c));
			const ConstraintId3D pin = world->CreateConstraint(ConstraintParams3D(ConstraintType3D::Point, a, k_InvalidBody3D));
			world->DestroyBody(a);
			Check(!world->IsConstraintValid(ab) && !world->IsConstraintValid(pin), "destroying body A destroys its constraints, world pins included");
			Check(world->IsConstraintValid(bc), "destroying a body leaves other constraints alone");
			world->DestroyBody(c);
			Check(!world->IsConstraintValid(bc), "destroying body B destroys the constraint");
			Check(!world->IsCollisionIgnored(b, c) && world->IsBodyValid(b), "destroying a body leaves no pair ignore behind");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			const PhysicsBodyId3D a = Box(*world, BodyType3D::Dynamic, glm::vec3(0.0f));
			const PhysicsBodyId3D b = Box(*world, BodyType3D::Dynamic, { 3.0f, 0.0f, 0.0f });
			const ConstraintId3D id = world->CreateConstraint(ConstraintParams3D(ConstraintType3D::Point, a, b));
			world->DestroyConstraint(id);
			Check(!world->IsConstraintValid(id), "DestroyConstraint invalidates the handle");
			const ConstraintId3D next = world->CreateConstraint(ConstraintParams3D(ConstraintType3D::Point, a, b));
			Check(next != id && !world->IsConstraintValid(id), "constraint handles are never reused");
			world->Step(k_Dt, 1);
			for (const ConstraintId3D stale : { id, k_InvalidConstraint3D, ConstraintId3D(123456) })
			{
				world->DestroyConstraint(stale);
				world->SetConstraintEnabled(stale, false);
				const bool inert = !world->IsConstraintValid(stale) && !world->IsConstraintEnabled(stale)
					&& world->GetConstraintBody(stale, 0) == k_InvalidBody3D && world->GetConstraintBody(stale, 1) == k_InvalidBody3D
					&& world->GetConstraintValue(stale) == 0.0f && world->GetConstraintUserData(stale) == 0;
				Check(inert, stale == id ? "a stale constraint handle is a no-op on every call" : stale == k_InvalidConstraint3D ? "k_InvalidConstraint3D is a no-op on every call" : "a made-up constraint handle is a no-op on every call");
			}
			Check(world->IsConstraintValid(next) && world->GetConstraintBody(next, 2) == k_InvalidBody3D, "an out-of-range body index reports none");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			const PhysicsBodyId3D a = Box(*world, BodyType3D::Dynamic, glm::vec3(0.0f));
			const PhysicsBodyId3D b = Box(*world, BodyType3D::Dynamic, { 3.0f, 0.0f, 0.0f });
			const PhysicsBodyId3D fixedBody = Box(*world, BodyType3D::Static, { 9.0f, 0.0f, 0.0f });
			const PhysicsBodyId3D gone = Box(*world, BodyType3D::Dynamic, { 12.0f, 0.0f, 0.0f });
			world->DestroyBody(gone);

			auto rejected = [&](ConstraintParams3D params) { return world->CreateConstraint(params) == k_InvalidConstraint3D; };
			Check(rejected(ConstraintParams3D(ConstraintType3D::Point, gone, b)), "create fails for a stale BodyA");
			Check(rejected(ConstraintParams3D(ConstraintType3D::Point, a, gone)), "create fails for a stale BodyB");
			Check(rejected(ConstraintParams3D(ConstraintType3D::Point, k_InvalidBody3D, b)), "create fails without a BodyA");
			Check(rejected(ConstraintParams3D(ConstraintType3D::Point, a, a)), "create fails for a body joined to itself");
			Check(rejected(ConstraintParams3D(ConstraintType3D::Point, fixedBody, b)) == false, "create accepts a static BodyA with a dynamic BodyB");
			Check(rejected(ConstraintParams3D(ConstraintType3D::Point, fixedBody, k_InvalidBody3D)), "create fails for a static body pinned to the world");
			ConstraintParams3D zeroAxis(ConstraintType3D::Hinge, a, b);
			zeroAxis.AxisA = glm::vec3(0.0f);
			Check(rejected(zeroAxis), "create fails for a zero axis the type reads");
			ConstraintParams3D parallel(ConstraintType3D::Hinge, a, b);
			parallel.NormalAxisA = parallel.AxisA;
			Check(rejected(parallel), "create fails for a normal axis parallel to the axis");
			ConstraintParams3D hingeLow(ConstraintType3D::Hinge, a, b);
			hingeLow.MinLimit = -4.0f;
			hingeLow.MaxLimit = 1.0f;
			Check(rejected(hingeLow), "create fails for a hinge limit outside [-pi, pi]");
			ConstraintParams3D hingeSide(ConstraintType3D::Hinge, a, b);
			hingeSide.MinLimit = 0.5f;
			hingeSide.MaxLimit = 1.0f;
			Check(rejected(hingeSide), "create fails for a hinge range that excludes zero");
			ConstraintParams3D negative(ConstraintType3D::Distance, a, b);
			negative.MinLimit = -1.0f;
			negative.MaxLimit = 2.0f;
			Check(rejected(negative), "create fails for a negative distance");
			ConstraintParams3D inverted(ConstraintType3D::Distance, a, b);
			inverted.MinLimit = 3.0f;
			inverted.MaxLimit = 2.0f;
			Check(rejected(inverted), "create fails for a distance with Min > Max");
			ConstraintParams3D twist(ConstraintType3D::SwingTwist, a, b);
			twist.SwingLimitY = twist.SwingLimitZ = 0.5f;
			twist.TwistMin = 0.4f;
			twist.TwistMax = -0.4f;
			Check(rejected(twist), "create fails for a twist range with Min > Max");
			ConstraintParams3D slider(ConstraintType3D::Slider, a, b);
			slider.MinLimit = 0.5f;
			slider.MaxLimit = 1.0f;
			Check(rejected(slider), "create fails for a slider range that excludes zero");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			const PhysicsBodyId3D a = Box(*world, BodyType3D::Dynamic, glm::vec3(0.0f));
			const PhysicsBodyId3D b = Box(*world, BodyType3D::Dynamic, { 3.0f, 0.0f, 0.0f });
			const ConstraintId3D id = world->CreateConstraint(ConstraintParams3D(ConstraintType3D::Point, a, b));
			world->Shutdown();
			Check(!world->IsConstraintValid(id), "Shutdown removes constraints with the bodies");
		}
	}

	void ConstraintTest::RunSceneChecks()
	{
		Scene scene("Constraint Scene Check");

		auto Dynamic = [&](const char* name, const glm::vec3& position)
		{
			Entity entity = scene.CreateEntity(name);
			entity.AddComponent<Transform3DComponent>(Transform3DComponent(position));
			entity.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Dynamic));
			return entity;
		};

		Entity pinned = Dynamic("Pinned", { 0.0f, 5.0f, 0.0f });
		Constraint3DComponent pin(ConstraintType3D::Fixed);
		pin.AnchorB = { 0.0f, 5.0f, 0.0f };
		pinned.AddComponent<Constraint3DComponent>(pin);

		Entity swinger = Dynamic("Swinger", { 3.0f, 5.0f, 0.0f });
		Constraint3DComponent joint(ConstraintType3D::Point, pinned.GetUUID());
		joint.AnchorA = { -3.0f, 0.0f, 0.0f };
		swinger.AddComponent<Constraint3DComponent>(joint);

		Entity group = scene.CreateEntity("Group");
		group.AddComponent<Transform3DComponent>(Transform3DComponent({ 0.0f, 30.0f, 0.0f }));
		Entity childA = Dynamic("Child A", { 10.0f, 30.0f, 0.0f });
		Entity childB = Dynamic("Child B", { 13.0f, 30.0f, 0.0f });
		childA.SetParent(group);
		childB.SetParent(group);
		childA.AddComponent<Constraint3DComponent>(Constraint3DComponent(ConstraintType3D::Point, childB.GetUUID()));

		scene.OnStart();

		float lowest = 5.0f, worstRadius = 0.0f;
		for (int i = 0; i < 120; i++)
		{
			scene.OnUpdate(1.0f / 60.0f);
			const glm::vec3 p = swinger.GetComponent<Transform3DComponent>().Position;
			lowest = (std::min)(lowest, p.y);
			worstRadius = (std::max)(worstRadius, std::abs(Distance(p, pinned.GetComponent<Transform3DComponent>().Position) - 3.0f));
		}

		const glm::vec3 pinnedAt = pinned.GetComponent<Transform3DComponent>().Position;
		Check(Distance(pinnedAt, { 0.0f, 5.0f, 0.0f }) < 0.05f, "scene: a Fixed world constraint holds its entity at OnStart");
		Check(worstRadius < 0.1f && lowest < 3.0f, "scene: a Point constraint to another entity swings it about the joint");

		Entity copy = scene.DuplicateEntity(swinger);
		Check(copy.IsValid() && copy.HasComponent<Constraint3DComponent>() && copy.GetComponent<Constraint3DComponent>().Other == pinned.GetUUID(),
			"scene: duplicate keeps an Other outside the copied subtree");

		Entity groupCopy = scene.DuplicateEntity(group);
		bool remapped = false;
		std::vector<Entity> copies = groupCopy.GetChildren();
		std::vector<Entity> originals = group.GetChildren();
		for (Entity candidate : copies)
		{
			if (!candidate.HasComponent<Constraint3DComponent>())
				continue;
			const UUID other = candidate.GetComponent<Constraint3DComponent>().Other;
			bool isCopy = false, isOriginal = false;
			for (Entity c : copies)
				isCopy = isCopy || c.GetUUID() == other;
			for (Entity o : originals)
				isOriginal = isOriginal || o.GetUUID() == other;
			remapped = isCopy && !isOriginal;
		}
		Check(remapped, "scene: duplicate remaps an Other inside the copied subtree to the copy");

		for (int i = 0; i < 60; i++)
			scene.OnUpdate(1.0f / 60.0f);

		scene.DestroyEntity(pinned);
		for (int i = 0; i < 120; i++)
			scene.OnUpdate(1.0f / 60.0f);
		Check(swinger.IsValid() && swinger.GetComponent<Transform3DComponent>().Position.y < -5.0f,
			"scene: destroying the Other entity frees the swinger from its constraint");

		Entity held = Dynamic("Held", { 0.0f, 50.0f, 0.0f });
		Constraint3DComponent holdPin(ConstraintType3D::Fixed);
		holdPin.AnchorB = { 0.0f, 50.0f, 0.0f };
		held.AddComponent<Constraint3DComponent>(holdPin);
		scene.CreateRigidBody(held);
		for (int i = 0; i < 60; i++)
			scene.OnUpdate(1.0f / 60.0f);
		Check(std::abs(held.GetComponent<Transform3DComponent>().Position.y - 50.0f) < 0.1f, "scene: a constraint added while running is built on the next update");

		held.RemoveComponent<Constraint3DComponent>();
		for (int i = 0; i < 30; i++)
			scene.OnUpdate(1.0f / 60.0f);
		Check(held.GetComponent<Transform3DComponent>().Position.y < 49.0f, "scene: removing the component releases the body");

		Entity toggled = Dynamic("Toggled", { 20.0f, 50.0f, 0.0f });
		Constraint3DComponent togglePin(ConstraintType3D::Fixed);
		togglePin.AnchorB = { 20.0f, 50.0f, 0.0f };
		toggled.AddComponent<Constraint3DComponent>(togglePin);
		scene.CreateRigidBody(toggled);
		for (int i = 0; i < 30; i++)
			scene.OnUpdate(1.0f / 60.0f);
		toggled.SetActive(false);
		toggled.SetActive(true);
		for (int i = 0; i < 60; i++)
			scene.OnUpdate(1.0f / 60.0f);
		Check(toggled.IsValid() && toggled.GetComponent<Transform3DComponent>().Position.y > 49.0f, "scene: SetActive false then true rebuilds the constraint");
	}

	void ConstraintTest::RunMotorChecks()
	{
		const glm::vec3 gravity(0.0f, -9.81f, 0.0f);

		{
			Arm arm = MakeArm(gravity);
			arm.World->SetMotor(arm.Joint, MotorAxis3D::Hinge, MotorMode3D::Position, k_Hold);
			float torque = 0.0f;
			for (int i = 0; i < 60; i++)
			{
				arm.World->Step(k_Dt, 1);
				torque = (std::max)(torque, arm.World->GetMotorTorque(arm.Joint, MotorAxis3D::Hinge));
			}
			Run(*arm.World, 2.0f);
			Check(arm.World->IsConstraintValid(arm.Joint) && std::abs(Angle(arm)) < 0.1f, "motor: a Position motor holds a loaded arm against gravity");
			Check(torque > 500.0f && torque < k_Hold.MaxTorque, "motor: GetMotorTorque reports the holding torque");
			Check(arm.World->GetMotorMode(arm.Joint, MotorAxis3D::Hinge) == MotorMode3D::Position && arm.World->GetMotorParams(arm.Joint, MotorAxis3D::Hinge) == k_Hold,
				"motor: the mode and params read back as set");
		}
		{
			Arm arm = MakeArm(gravity);
			MotorParams3D off = k_Hold;
			off.MaxTorque = 0.0f;
			arm.World->SetMotor(arm.Joint, MotorAxis3D::Hinge, MotorMode3D::Position, off);
			Run(*arm.World, 1.0f);
			Check(std::abs(Angle(arm)) > 0.8f && arm.World->GetMotorTorque(arm.Joint, MotorAxis3D::Hinge) == 0.0f,
				"motor: MaxTorque 0 leaves the joint passive");

			Arm offMode = MakeArm(gravity);
			offMode.World->SetMotor(offMode.Joint, MotorAxis3D::Hinge, MotorMode3D::Off, k_Hold);
			Run(*offMode.World, 1.0f);
			Check(std::abs(Angle(offMode)) > 0.8f && offMode.World->GetMotorTorque(offMode.Joint, MotorAxis3D::Hinge) == 0.0f,
				"motor: mode Off leaves the joint passive");
		}
		{
			Arm arm = MakeArm(gravity);
			arm.World->SetMotor(arm.Joint, MotorAxis3D::Hinge, MotorMode3D::Position, k_Hold);
			arm.World->SetMotorTargetRotation(arm.Joint, AboutZ(0.6f));
			float reachedAt = -1.0f;
			for (int i = 0; i < 480; i++)
			{
				arm.World->Step(k_Dt, 1);
				if (reachedAt < 0.0f && std::abs(Angle(arm) - 0.6f) < 0.05f)
					reachedAt = static_cast<float>(i + 1) * k_Dt;
			}
			Check(reachedAt > 0.0f && reachedAt < 1.5f, "motor: a Position motor reaches its target within a set time");
			Check(std::abs(Angle(arm) - 0.6f) < 0.05f, "motor: a Position motor settles on its target");
		}
		{
			Arm arm = MakeArm(gravity, -0.5f, 0.5f);
			arm.World->SetMotor(arm.Joint, MotorAxis3D::Hinge, MotorMode3D::Position, k_Hold);
			arm.World->SetMotorTargetRotation(arm.Joint, AboutZ(1.2f));
			Run(*arm.World, 2.0f);
			Check(std::abs(Angle(arm) - 0.5f) < 0.05f, "motor: a Position target past a hinge limit pushes to the limit");
		}
		{
			Arm arm = MakeArm(glm::vec3(0.0f));
			arm.World->SetMotor(arm.Joint, MotorAxis3D::Hinge, MotorMode3D::Velocity, { 0.0f, 0.0f, 100000.0f });
			arm.World->SetMotorTargetVelocity(arm.Joint, { 0.0f, 0.0f, 2.0f });
			Run(*arm.World, 1.0f);
			Check(std::abs(arm.World->GetAngularVelocity(arm.Body).z - 2.0f) < 0.1f, "motor: a Velocity drive spins at the target rate");
			arm.World->SetMotorTargetVelocity(arm.Joint, { 0.0f, 0.0f, -3.0f });
			Run(*arm.World, 1.0f);
			Check(std::abs(arm.World->GetAngularVelocity(arm.Body).z + 3.0f) < 0.1f, "motor: a Velocity drive reverses with the target");
			arm.World->SetMotorTargetVelocity(arm.Joint, { 5.0f, 5.0f, 1.0f });
			Run(*arm.World, 1.0f);
			Check(std::abs(arm.World->GetAngularVelocity(arm.Body).z - 1.0f) < 0.1f, "motor: a hinge Velocity drive uses the component along its axis");
		}
		{
			Arm arm = MakeArm(glm::vec3(0.0f));
			arm.World->SetMotor(arm.Joint, MotorAxis3D::Hinge, MotorMode3D::Velocity, { 0.0f, 5000.0f, 100000.0f });
			arm.World->SetMotorTargetVelocity(arm.Joint, { 0.0f, 0.0f, 1.5f });
			Run(*arm.World, 3.0f);
			Check(std::abs(arm.World->GetAngularVelocity(arm.Body).z - 1.5f) < 0.05f, "motor: a damped Velocity drive approaches the target rate");
		}
		{
			Arm arm = MakeArm(gravity);
			const MotorParams3D weak{ 100000.0f, 1000.0f, 800.0f };
			arm.World->SetMotor(arm.Joint, MotorAxis3D::Hinge, MotorMode3D::Position, weak);
			arm.World->SetMotorTargetRotation(arm.Joint, AboutZ(1.5f));
			float peak = 0.0f;
			for (int i = 0; i < 240; i++)
			{
				arm.World->Step(k_Dt, 1);
				peak = (std::max)(peak, arm.World->GetMotorTorque(arm.Joint, MotorAxis3D::Hinge));
			}
			Check(peak <= weak.MaxTorque * 1.02f && peak > weak.MaxTorque * 0.9f, "motor: the torque saturates at MaxTorque and never exceeds it");
		}
		{
			Arm arm = MakeArm(glm::vec3(0.0f));
			arm.World->SetMotorTargetVelocity(arm.Joint, { 0.0f, 0.0f, 2.0f });
			arm.World->SetMotor(arm.Joint, MotorAxis3D::Hinge, MotorMode3D::Velocity, { 0.0f, 5000.0f, 100000.0f });
			arm.World->SetMotorTargetVelocity(arm.Joint, { 0.0f, 0.0f, 0.0f });
			Run(*arm.World, 3.0f);
			arm.World->SetMotorTargetVelocity(arm.Joint, { 0.0f, 0.0f, 2.0f });
			Run(*arm.World, 3.0f);
			Check(std::abs(arm.World->GetAngularVelocity(arm.Body).z - 2.0f) < 0.1f, "motor: a new velocity target set on a sleeping pair takes effect on the next step");
		}
		{
			Arm arm = MakeArm(glm::vec3(0.0f));
			Run(*arm.World, 3.0f);
			arm.World->SetMotor(arm.Joint, MotorAxis3D::Hinge, MotorMode3D::Velocity, { 0.0f, 0.0f, 100000.0f });
			arm.World->SetMotorTargetVelocity(arm.Joint, { 0.0f, 0.0f, 2.0f });
			Run(*arm.World, 1.0f);
			Check(std::abs(arm.World->GetAngularVelocity(arm.Body).z - 2.0f) < 0.1f, "motor: switching a sleeping pair's motor on wakes it");
		}
		{
			Arm arm = MakeArm(glm::vec3(0.0f));
			arm.World->SetMotor(arm.Joint, MotorAxis3D::Hinge, MotorMode3D::Position, k_Hold);
			Run(*arm.World, 3.0f);
			arm.World->SetMotorTargetRotation(arm.Joint, AboutZ(0.5f));
			Run(*arm.World, 2.0f);
			Check(std::abs(Angle(arm) - 0.5f) < 0.05f, "motor: a new rotation target set on a sleeping pair takes effect on the next step");
		}
		{
			Arm arm = MakeArm(gravity);
			arm.World->SetMotor(arm.Joint, MotorAxis3D::Hinge, MotorMode3D::Position, k_Hold);
			arm.World->SetMotor(arm.Joint, MotorAxis3D::Hinge, MotorMode3D::Position, { -1.0f, 10.0f, 10.0f });
			arm.World->SetMotor(arm.Joint, MotorAxis3D::Hinge, MotorMode3D::Position, { 1.0f, -10.0f, 10.0f });
			arm.World->SetMotor(arm.Joint, MotorAxis3D::Hinge, MotorMode3D::Position, { 1.0f, 10.0f, -10.0f });
			arm.World->SetMotor(arm.Joint, MotorAxis3D::Hinge, MotorMode3D::Position, { std::nanf(""), 10.0f, 10.0f });
			arm.World->SetMotorTargetRotation(arm.Joint, glm::quat(std::nanf(""), 0.0f, 0.0f, 0.0f));
			arm.World->SetMotorTargetVelocity(arm.Joint, glm::vec3(std::nanf(""), 0.0f, 0.0f));
			Check(arm.World->GetMotorParams(arm.Joint, MotorAxis3D::Hinge) == k_Hold && arm.World->GetMotorMode(arm.Joint, MotorAxis3D::Hinge) == MotorMode3D::Position,
				"motor: negative and NaN params are ignored");
			Run(*arm.World, 2.0f);
			Check(std::abs(Angle(arm)) < 0.1f, "motor: the motor keeps its last good settings after a rejected call");

			arm.World->SetMotor(arm.Joint, MotorAxis3D::Swing, MotorMode3D::Velocity, k_Hold);
			arm.World->SetMotor(arm.Joint, MotorAxis3D::Twist, MotorMode3D::Velocity, k_Hold);
			Check(arm.World->GetMotorMode(arm.Joint, MotorAxis3D::Swing) == MotorMode3D::Off && arm.World->GetMotorMode(arm.Joint, MotorAxis3D::Twist) == MotorMode3D::Off
				&& arm.World->GetMotorTorque(arm.Joint, MotorAxis3D::Swing) == 0.0f && arm.World->GetMotorParams(arm.Joint, MotorAxis3D::Twist) == MotorParams3D(),
				"motor: a Hinge ignores the Swing and Twist axes");
			Check(arm.World->GetMotorMode(arm.Joint, MotorAxis3D::Hinge) == MotorMode3D::Position, "motor: an ignored axis leaves the other motor alone");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			const PhysicsBodyId3D a = Box(*world, BodyType3D::Static, glm::vec3(0.0f));
			const PhysicsBodyId3D b = Box(*world, BodyType3D::Dynamic, { 0.0f, -1.0f, 0.0f });
			ConstraintParams3D params(ConstraintType3D::SwingTwist, a, b);
			params.AnchorB = { 0.0f, 1.0f, 0.0f };
			params.SwingLimitY = params.SwingLimitZ = 0.8f;
			params.TwistMin = -3.0f;
			params.TwistMax = 3.0f;
			const ConstraintId3D joint = world->CreateConstraint(params);

			world->SetMotor(joint, MotorAxis3D::Hinge, MotorMode3D::Velocity, k_Hold);
			Check(world->GetMotorMode(joint, MotorAxis3D::Hinge) == MotorMode3D::Off, "motor: a SwingTwist ignores the Hinge axis");

			world->SetMotor(joint, MotorAxis3D::Twist, MotorMode3D::Velocity, { 0.0f, 0.0f, 100000.0f });
			world->SetMotorTargetVelocity(joint, { 0.0f, 1.5f, 0.0f });
			Run(*world, 1.0f);
			Check(std::abs(world->GetAngularVelocity(b).y - 1.5f) < 0.1f, "motor: a SwingTwist twist motor spins at the target rate");
			Check(world->GetMotorTorque(joint, MotorAxis3D::Swing) == 0.0f && world->GetMotorMode(joint, MotorAxis3D::Swing) == MotorMode3D::Off,
				"motor: the SwingTwist swing motor stays off when only twist is set");

		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			const PhysicsBodyId3D a = Box(*world, BodyType3D::Static, glm::vec3(0.0f));
			const PhysicsBodyId3D b = Box(*world, BodyType3D::Dynamic, { 0.0f, -1.0f, 0.0f });
			ConstraintParams3D params(ConstraintType3D::SwingTwist, a, b);
			params.AnchorB = { 0.0f, 1.0f, 0.0f };
			params.SwingLimitY = params.SwingLimitZ = 0.8f;
			params.TwistMin = -3.0f;
			params.TwistMax = 3.0f;
			const ConstraintId3D joint = world->CreateConstraint(params);
			world->SetMotor(joint, MotorAxis3D::Swing, MotorMode3D::Position, k_Hold);
			world->SetMotorTargetRotation(joint, glm::angleAxis(0.4f, glm::vec3(1.0f, 0.0f, 0.0f)));
			Run(*world, 2.0f);
			const glm::quat rotation = world->GetRotation(b);
			Check(std::abs(2.0f * std::atan2(rotation.x, rotation.w) - 0.4f) < 0.06f, "motor: a SwingTwist swing motor drives toward a rotation target");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			const PhysicsBodyId3D a = Box(*world, BodyType3D::Static, glm::vec3(0.0f));
			const PhysicsBodyId3D b = Box(*world, BodyType3D::Dynamic, { 3.0f, 0.0f, 0.0f });
			const ConstraintId3D point = world->CreateConstraint(ConstraintParams3D(ConstraintType3D::Point, a, b));
			world->SetMotor(point, MotorAxis3D::Hinge, MotorMode3D::Velocity, k_Hold);
			world->SetMotorTargetVelocity(point, { 0.0f, 0.0f, 2.0f });
			Run(*world, 0.5f);
			Check(world->GetMotorMode(point, MotorAxis3D::Hinge) == MotorMode3D::Off && std::abs(world->GetAngularVelocity(b).z) < 0.01f,
				"motor: a joint without motors ignores them");

			for (const ConstraintId3D stale : { k_InvalidConstraint3D, ConstraintId3D(123456) })
			{
				world->SetMotor(stale, MotorAxis3D::Hinge, MotorMode3D::Position, k_Hold);
				world->SetMotorTargetRotation(stale, AboutZ(1.0f));
				world->SetMotorTargetVelocity(stale, k_AxisZ);
				Check(world->GetMotorMode(stale, MotorAxis3D::Hinge) == MotorMode3D::Off && world->GetMotorParams(stale, MotorAxis3D::Hinge) == MotorParams3D()
					&& world->GetMotorTorque(stale, MotorAxis3D::Hinge) == 0.0f, "motor: a stale handle is a no-op on every motor call");
			}
		}
	}

	void ConstraintTest::RunMotorSceneChecks()
	{
		Scene scene("Constraint Motor Scene Check");

		Entity anchor = scene.CreateEntity("Motor Anchor");
		anchor.AddComponent<Transform3DComponent>(Transform3DComponent(glm::vec3(0.0f), glm::vec3(0.5f)));
		anchor.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Static));
		Entity arm = scene.CreateEntity("Motor Arm");
		arm.AddComponent<Transform3DComponent>(Transform3DComponent({ 2.0f, 0.0f, 0.0f }, glm::vec3(0.5f)));
		arm.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Dynamic));

		Constraint3DComponent joint(ConstraintType3D::Hinge, arm.GetUUID());
		joint.AnchorB = { -2.0f, 0.0f, 0.0f };
		joint.AxisA = joint.AxisB = k_AxisZ;
		joint.MotorMode = MotorMode3D::Position;
		joint.Motor = k_Hold;
		anchor.AddComponent<Constraint3DComponent>(joint);

		scene.OnStart();
		auto angle = [&] { return AngleOf(arm.GetComponent<Transform3DComponent>().Rotation); };
		auto run = [&](int frames) { for (int i = 0; i < frames; i++) scene.OnUpdate(1.0f / 60.0f); };

		run(120);
		Check(std::abs(angle()) < 0.1f, "scene: a motor on the component holds the arm from OnStart");

		anchor.GetComponent<Constraint3DComponent>().TargetRotation = AboutZ(0.5f);
		run(120);
		Check(std::abs(angle() - 0.5f) < 0.06f, "scene: editing TargetRotation retunes the live motor without a rebuild");

		anchor.GetComponent<Constraint3DComponent>().MotorMode = MotorMode3D::Velocity;
		anchor.GetComponent<Constraint3DComponent>().Motor = { 0.0f, 0.0f, 100000.0f };
		anchor.GetComponent<Constraint3DComponent>().TargetAngularVelocity = { 0.0f, 0.0f, -1.0f };
		run(30);
		Check(angle() < 0.5f - 0.5f * 0.5f, "scene: switching the component to Velocity drives the arm at the target rate");

		anchor.GetComponent<Constraint3DComponent>().MotorMode = MotorMode3D::Off;
		anchor.GetComponent<Constraint3DComponent>().TargetAngularVelocity = glm::vec3(0.0f);
		run(30);
		Check(angle() < -0.3f, "scene: switching the component's motor Off lets gravity take the arm");
	}

	void ConstraintTest::RunGroupChecks()
	{
		auto overlapping = [](Physics3D& world, std::uint32_t groupA, bool withinA, std::uint32_t groupB, bool withinB)
		{
			const PhysicsBodyId3D a = Box(world, BodyType3D::Dynamic, glm::vec3(0.0f));
			const PhysicsBodyId3D b = Box(world, BodyType3D::Dynamic, { 0.5f, 0.0f, 0.0f });
			world.SetCollisionGroup(a, groupA, withinA);
			world.SetCollisionGroup(b, groupB, withinB);
			Run(world, 1.0f);
			return Distance(world.GetPosition(a), world.GetPosition(b));
		};

		{
			auto world = MakeWorld(glm::vec3(0.0f));
			Check(overlapping(*world, 5, false, 5, false) < 0.6f, "group: bodies in the same group pass through each other");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			Check(overlapping(*world, 5, false, 6, false) > 0.95f, "group: bodies in different groups collide");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			Check(overlapping(*world, 5, false, k_NoCollisionGroup, false) > 0.95f, "group: a grouped body collides with an ungrouped one");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			Check(overlapping(*world, 0xFFFFFFFFu, false, 0xFFFFFFFFu, false) < 0.6f, "group: any uint32 is a legal group");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			Check(overlapping(*world, 5, true, 5, true) > 0.95f, "group: bodies that both opt in collide within their group");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			Check(overlapping(*world, 5, true, 5, false) < 0.6f, "group: a one-sided opt-in still passes through");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			const PhysicsBodyId3D a = Box(*world, BodyType3D::Dynamic, glm::vec3(0.0f));
			const PhysicsBodyId3D b = Box(*world, BodyType3D::Dynamic, { 0.5f, 0.0f, 0.0f });
			world->SetCollisionGroup(a, 7, true);
			world->SetCollisionGroup(b, 7, true);
			Check(world->GetCollisionGroup(a) == 7 && world->GetCollideWithinGroup(a) && world->GetCollisionGroup(b) == 7, "group: getters report the group and the opt-in");
			world->SetCollisionGroup(a, k_NoCollisionGroup, true);
			Check(world->GetCollisionGroup(a) == k_NoCollisionGroup && !world->GetCollideWithinGroup(a), "group: group 0 leaves the group and clears the opt-in");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			const PhysicsBodyId3D a = Box(*world, BodyType3D::Dynamic, glm::vec3(0.0f));
			const PhysicsBodyId3D b = Box(*world, BodyType3D::Dynamic, { 0.5f, 0.0f, 0.0f });
			world->SetCollisionGroup(a, 4);
			world->SetCollisionGroup(b, 4);
			Run(*world, 0.5f);
			Check(Distance(world->GetPosition(a), world->GetPosition(b)) < 0.6f, "group: the pair overlaps while grouped");
			world->SetCollisionGroup(a, k_NoCollisionGroup);
			Run(*world, 1.0f);
			Check(Distance(world->GetPosition(a), world->GetPosition(b)) > 0.95f, "group: a body moved to group 0 collides again from the next Step");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			std::vector<PhysicsBodyId3D> bodies;
			for (int i = 0; i < 15; i++)
				bodies.push_back(Box(*world, BodyType3D::Dynamic, { i * 3.0f, 0.0f, 0.0f }));
			for (PhysicsBodyId3D body : bodies)
				world->SetCollisionGroup(body, 9);
			bool anyIgnored = false;
			for (size_t i = 0; i < bodies.size(); i++)
				for (size_t j = i + 1; j < bodies.size(); j++)
					anyIgnored = anyIgnored || world->IsCollisionIgnored(bodies[i], bodies[j]);
			Check(!anyIgnored, "group: 15 bodies in one group leave no IgnoreCollision entries");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			const PhysicsBodyId3D a = Box(*world, BodyType3D::Dynamic, glm::vec3(0.0f));
			const PhysicsBodyId3D b = Box(*world, BodyType3D::Dynamic, { 0.5f, 0.0f, 0.0f });
			world->SetCollisionGroup(a, 3);
			world->SetCollisionGroup(b, 3);
			std::vector<PhysicsBodyId3D> hits;
			world->OverlapSphere({ 0.25f, 0.0f, 0.0f }, 0.1f, hits);
			const bool both = std::find(hits.begin(), hits.end(), a) != hits.end() && std::find(hits.begin(), hits.end(), b) != hits.end();
			Check(both, "group: OverlapSphere still reports grouped bodies");
			RayCastHit3D hit;
			Check(world->RayCast(Ray({ -3.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }), 10.0f, hit) && hit.Body == a, "group: RayCast still hits a grouped body");
			RayCastHit3D swept;
			Check(world->ShapeCastSphere({ -3.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, 0.1f, 10.0f, swept) && swept.Body == a, "group: ShapeCastSphere still hits a grouped body");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			const PhysicsBodyId3D a = Box(*world, BodyType3D::Dynamic, glm::vec3(0.0f));
			const PhysicsBodyId3D b = Box(*world, BodyType3D::Dynamic, { 0.5f, 0.0f, 0.0f });
			world->SetCollisionGroup(a, 2);
			world->SetCollisionGroup(b, 2);
			world->IgnoreCollision(a, b, true);
			Check(world->IsCollisionIgnored(a, b), "group + ignore: the pair is ignored");
			world->IgnoreCollision(a, b, false);
			Check(!world->IsCollisionIgnored(a, b) && world->GetCollisionGroup(a) == 2 && world->GetCollisionGroup(b) == 2,
				"group + ignore: the group survives the pair's removal");
			Run(*world, 1.0f);
			Check(Distance(world->GetPosition(a), world->GetPosition(b)) < 0.6f, "group + ignore: the group still passes through after the pair is removed");
			world->SetCollisionGroup(a, k_NoCollisionGroup);
			world->SetCollisionGroup(b, k_NoCollisionGroup);
			Run(*world, 1.0f);
			Check(Distance(world->GetPosition(a), world->GetPosition(b)) > 0.95f, "group + ignore: leaving the group collides again");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			const PhysicsBodyId3D a = Box(*world, BodyType3D::Dynamic, glm::vec3(0.0f));
			const PhysicsBodyId3D b = Box(*world, BodyType3D::Dynamic, { 0.5f, 0.0f, 0.0f });
			world->SetCollisionGroup(a, 2);
			world->SetCollisionGroup(b, 2);
			world->IgnoreCollision(a, b, true);
			world->SetCollisionGroup(a, k_NoCollisionGroup);
			world->SetCollisionGroup(b, k_NoCollisionGroup);
			Check(world->IsCollisionIgnored(a, b), "group + ignore: the pair survives leaving the group");
			Run(*world, 1.0f);
			Check(Distance(world->GetPosition(a), world->GetPosition(b)) < 0.6f, "group + ignore: the pair still passes through after the group is removed");
			world->IgnoreCollision(a, b, false);
			Run(*world, 1.0f);
			Check(Distance(world->GetPosition(a), world->GetPosition(b)) > 0.95f, "group + ignore: removing the last mechanism collides again");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			const PhysicsBodyId3D a = Box(*world, BodyType3D::Dynamic, glm::vec3(0.0f));
			const PhysicsBodyId3D b = Box(*world, BodyType3D::Dynamic, { 0.5f, 0.0f, 0.0f });
			const PhysicsBodyId3D c = Box(*world, BodyType3D::Dynamic, { 0.0f, 0.6f, 0.0f });
			world->SetCollisionGroup(a, 8);
			ConstraintParams3D params(ConstraintType3D::Distance, a, b);
			params.MaxLimit = 5.0f;
			const ConstraintId3D id = world->CreateConstraint(params);
			Check(world->IsConstraintValid(id) && world->IsCollisionIgnored(a, b) && world->GetCollisionGroup(a) == 8,
				"group: CollideConnected false still ignores the pair of a grouped body");
			Run(*world, 1.0f);
			Check(Distance(world->GetPosition(a), world->GetPosition(b)) < 0.6f, "group: a constraint's pass-through works on a grouped body");
			Check(Distance(world->GetPosition(a), world->GetPosition(c)) > 0.95f, "group: a grouped body still collides with an ungrouped third body");
			world->DestroyConstraint(id);
			Check(world->GetCollisionGroup(a) == 8, "group: destroying the constraint keeps the group");
			const PhysicsBodyId3D d = Box(*world, BodyType3D::Dynamic, { 0.0f, -0.6f, 0.0f });
			world->SetCollisionGroup(d, 8);
			Run(*world, 1.0f);
			Check(Distance(world->GetPosition(a), world->GetPosition(d)) < 0.6f, "group: after the constraint is gone the group still passes through");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			const PhysicsBodyId3D a = Box(*world, BodyType3D::Dynamic, glm::vec3(0.0f));
			const PhysicsBodyId3D b = Box(*world, BodyType3D::Dynamic, { 0.5f, 0.0f, 0.0f });
			const PhysicsBodyId3D c = Box(*world, BodyType3D::Dynamic, { 0.0f, 0.6f, 0.0f });
			world->SetCollisionGroup(a, 1);
			world->SetCollisionGroup(b, 1);
			world->SetCollisionGroup(c, 1);
			world->DestroyBody(c);
			Check(world->GetCollisionGroup(c) == k_NoCollisionGroup && !world->GetCollideWithinGroup(c), "group: a destroyed body reports no group");
			Run(*world, 0.5f);
			Check(world->IsBodyValid(a) && world->IsBodyValid(b) && Distance(world->GetPosition(a), world->GetPosition(b)) < 0.6f, "group: destroying a grouped body leaves the others grouped");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			RigidBodyParams3D sensor(BodyType3D::Static, ColliderShape3D::Box);
			sensor.HalfExtents = glm::vec3(2.0f);
			sensor.IsSensor = true;
			sensor.CollisionGroup = 6;
			const PhysicsBodyId3D trigger = world->CreateBody(sensor);
			const PhysicsBodyId3D inside = Box(*world, BodyType3D::Dynamic, glm::vec3(0.0f));
			world->SetCollisionGroup(inside, 6);
			Run(*world, 0.2f);
			std::vector<PhysicsBodyId3D> found;
			world->GetSensorOverlaps(trigger, found);
			Check(std::find(found.begin(), found.end(), inside) != found.end(), "group: a sensor still reports an overlapping body of its group");
		}
		{
			Mesh* floorMesh = Mesh::CreateBox();
			for (const bool sameGroup : { true, false })
			{
				auto world = MakeWorld({ 0.0f, -9.81f, 0.0f });
				RigidBodyParams3D floor(BodyType3D::Static, ColliderShape3D::Mesh);
				floor.Mesh = floorMesh;
				floor.MeshScale = { 10.0f, 1.0f, 10.0f };
				floor.CollisionGroup = 12;
				world->CreateBody(floor);
				RigidBodyParams3D faller(BodyType3D::Dynamic, ColliderShape3D::Box);
				faller.Position = { 0.0f, 3.0f, 0.0f };
				faller.CollisionGroup = sameGroup ? 12 : 13;
				const PhysicsBodyId3D body = world->CreateBody(faller);
				Run(*world, 2.0f);
				if (sameGroup)
					Check(world->GetPosition(body).y < -2.0f, "group: a body falls through a mesh collider of its group");
				else
					Check(world->GetPosition(body).y > 0.5f, "group: a body in another group lands on a mesh collider");
			}
			delete floorMesh;
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			const PhysicsBodyId3D a = Box(*world, BodyType3D::Dynamic, glm::vec3(0.0f));
			world->DestroyBody(a);
			world->SetCollisionGroup(a, 3, true);
			world->SetCollisionGroup(k_InvalidBody3D, 3);
			Check(world->GetCollisionGroup(a) == k_NoCollisionGroup && !world->GetCollideWithinGroup(a), "group: a stale handle reports no group");
			Check(world->GetCollisionGroup(k_InvalidBody3D) == k_NoCollisionGroup && !world->GetCollideWithinGroup(k_InvalidBody3D), "group: k_InvalidBody3D reports no group");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			const std::uint32_t first = world->AllocateCollisionGroup();
			const std::uint32_t second = world->AllocateCollisionGroup();
			Check(first != k_NoCollisionGroup && second != k_NoCollisionGroup && first != second, "group: AllocateCollisionGroup returns distinct non-zero ids");
			const PhysicsBodyId3D a = Box(*world, BodyType3D::Dynamic, glm::vec3(0.0f));
			world->SetCollisionGroup(a, second + 5);
			RigidBodyParams3D created(BodyType3D::Dynamic, ColliderShape3D::Box);
			created.CollisionGroup = second + 20;
			world->CreateBody(created);
			bool unique = true;
			for (int i = 0; i < 40; i++)
			{
				const std::uint32_t id = world->AllocateCollisionGroup();
				unique = unique && id != first && id != second && id != second + 5 && id > second + 20;
			}
			Check(unique, "group: AllocateCollisionGroup skips ids already used by SetCollisionGroup and CreateBody");
		}
	}

	void ConstraintTest::RunGroupSceneChecks()
	{
		Scene scene("Collision Group Scene Check");

		auto Body = [&](const char* name, const glm::vec3& position, std::uint32_t group, bool within)
		{
			Entity entity = scene.CreateEntity(name);
			entity.AddComponent<Transform3DComponent>(Transform3DComponent(position));
			RigidBody3DComponent body(BodyType3D::Dynamic);
			body.CollisionGroup = group;
			body.CollideWithinGroup = within;
			entity.AddComponent<RigidBody3DComponent>(body);
			return entity;
		};

		Entity floor = scene.CreateEntity("Floor");
		floor.AddComponent<Transform3DComponent>(Transform3DComponent({ 0.0f, -0.5f, 0.0f }, { 40.0f, 1.0f, 40.0f }));
		floor.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Static));

		Entity lower = Body("Lower", { 0.0f, 0.5f, 0.0f }, 21, false);
		Entity upper = Body("Upper", { 0.0f, 2.0f, 0.0f }, 21, false);
		Entity otherLower = Body("Other Lower", { 5.0f, 0.5f, 0.0f }, 22, false);
		Entity otherUpper = Body("Other Upper", { 5.0f, 2.0f, 0.0f }, 23, false);
		Entity optLower = Body("Opt Lower", { 10.0f, 0.5f, 0.0f }, 24, true);
		Entity optUpper = Body("Opt Upper", { 10.0f, 2.0f, 0.0f }, 24, true);

		scene.OnStart();
		for (int i = 0; i < 120; i++)
			scene.OnUpdate(1.0f / 60.0f);

		const auto y = [](Entity entity) { return entity.GetComponent<Transform3DComponent>().Position.y; };
		Check(y(upper) < 0.6f && std::abs(y(upper) - y(lower)) < 0.3f, "scene: entities in one group pass through each other");
		Check(y(otherUpper) > y(otherLower) + 0.8f, "scene: entities in different groups stack");
		Check(y(optUpper) > y(optLower) + 0.8f, "scene: entities that both opt in stack within their group");

		Entity copy = scene.DuplicateEntity(lower);
		Check(copy.IsValid() && copy.GetComponent<RigidBody3DComponent>().CollisionGroup == 21 && !copy.GetComponent<RigidBody3DComponent>().CollideWithinGroup,
			"scene: a duplicate stays in the same group");

		Entity late = Body("Late", { 0.0f, 4.0f, 0.0f }, 21, false);
		scene.CreateRigidBody(late);
		for (int i = 0; i < 120; i++)
			scene.OnUpdate(1.0f / 60.0f);
		Check(y(late) < 0.6f, "scene: a body created while running joins its group");
	}

	void ConstraintTest::RunBreakChecks()
	{
		constexpr float g = 9.81f;
		constexpr float loadMass = 200.0f;

		auto heavyBox = [](Physics3D& world, const glm::vec3& position, float halfExtent, float mass)
		{
			RigidBodyParams3D params;
			params.Type = BodyType3D::Dynamic;
			params.Shape = ColliderShape3D::Box;
			params.Position = position;
			params.HalfExtents = glm::vec3(halfExtent);
			params.Mass = mass;
			return world.CreateBody(params);
		};
		auto rod = [](PhysicsBodyId3D a, PhysicsBodyId3D b, float length, float breakForce, std::uint32_t userData)
		{
			ConstraintParams3D params(ConstraintType3D::Distance, a, b);
			params.MinLimit = params.MaxLimit = length;
			params.BreakForce = breakForce;
			params.UserData = userData;
			return params;
		};

		{
			auto world = MakeWorld({ 0.0f, -g, 0.0f });
			PhysicsBodyId3D bodies[7];
			bodies[0] = Box(*world, BodyType3D::Static, { 0.0f, 10.0f, 0.0f }, glm::vec3(0.1f));
			for (int i = 1; i < 6; i++)
				bodies[i] = heavyBox(*world, { 0.0f, 10.0f - i, 0.0f }, 0.1f, 1.0f);
			bodies[6] = heavyBox(*world, { 0.0f, 4.0f, 0.0f }, 0.3f, loadMass);

			ConstraintId3D links[6];
			for (int i = 0; i < 6; i++)
				links[i] = world->CreateConstraint(rod(bodies[i], bodies[i + 1], 1.0f, i == 3 ? 1500.0f : 3000.0f, 100 + i));

			std::vector<ConstraintBreak3D> broken;
			Run(*world, 4.0f);
			world->ConsumeBrokenConstraints(broken);
			Check(broken.size() == 1, "break: a rope with one weak link under a 200 kg load breaks exactly one link");
			if (broken.size() == 1)
			{
				Check(broken[0].Constraint == links[3] && broken[0].BodyA == bodies[3] && broken[0].BodyB == bodies[4] && broken[0].UserData == 103,
					"break: the report names the weak link, its bodies and its user data");
				Check(broken[0].Force > 1500.0f, "break: the reported force is above the threshold");
			}
			Check(!world->IsConstraintValid(links[3]), "break: the broken handle is stale");
			Check(world->IsConstraintValid(links[0]) && world->IsConstraintValid(links[2]) && world->IsConstraintValid(links[4]) && world->IsConstraintValid(links[5]),
				"break: the other links stay");
			std::vector<ConstraintBreak3D> again;
			world->ConsumeBrokenConstraints(again);
			Check(again.empty(), "break: a broken constraint is reported once");

			ConstraintParams3D fresh(ConstraintType3D::Distance, bodies[0], bodies[1]);
			fresh.MinLimit = fresh.MaxLimit = 1.0f;
			const ConstraintId3D next = world->CreateConstraint(fresh);
			Check(next != k_InvalidConstraint3D && next > links[3], "break: a new constraint's id is greater than the broken one's");
			Check(!world->IsConstraintValid(links[3]), "break: the broken handle stays stale after a new constraint");
		}
		{
			auto world = MakeWorld({ 0.0f, -g, 0.0f });
			const PhysicsBodyId3D anchor = Box(*world, BodyType3D::Static, { 0.0f, 10.0f, 0.0f }, glm::vec3(0.1f));
			const PhysicsBodyId3D load = heavyBox(*world, { 0.0f, 8.0f, 0.0f }, 0.3f, loadMass);
			const ConstraintId3D joint = world->CreateConstraint(rod(anchor, load, 2.0f, 0.0f, 0));
			Run(*world, 0.25f);
			float force = -1.0f, torque = -1.0f;
			Check(world->GetConstraintReaction(joint, force, torque), "reaction: a live constraint reports its reaction");
			Check(std::abs(force - loadMass * g) < 0.05f * loadMass * g, "reaction: a static hang reads about m*g");
			Check(torque == 0.0f, "reaction: a Distance constraint has no torque");
			Check(world->IsConstraintValid(joint), "reaction: a constraint without limits never breaks");
			world->SetConstraintEnabled(joint, false);
			world->GetConstraintReaction(joint, force, torque);
			Check(force == 0.0f, "reaction: a disabled constraint reads 0");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			const PhysicsBodyId3D anchor = Box(*world, BodyType3D::Static, { 0.0f, 10.0f, 0.0f }, glm::vec3(0.1f));
			const PhysicsBodyId3D load = heavyBox(*world, { 0.0f, 8.0f, 0.0f }, 0.3f, loadMass);
			const ConstraintId3D joint = world->CreateConstraint(rod(anchor, load, 2.0f, 1.0f, 0));
			Run(*world, 3.0f);
			world->SetGravity({ 0.0f, -g, 0.0f });
			Run(*world, 0.5f);
			float force = -1.0f, torque = -1.0f;
			world->GetConstraintReaction(joint, force, torque);
			Check(world->GetPosition(load).y > 7.999f, "reaction: the pair fell asleep and stays asleep when gravity changes");
			Check(force == 0.0f && torque == 0.0f, "reaction: a sleeping pair reads 0");
			Check(world->IsConstraintValid(joint), "break: a sleeping pair does not break");
		}
		{
			auto world = MakeWorld(glm::vec3(0.0f));
			const PhysicsBodyId3D anchor = Box(*world, BodyType3D::Static, { 0.0f, 10.0f, 0.0f }, glm::vec3(0.1f));
			const PhysicsBodyId3D load = heavyBox(*world, { 0.0f, 8.0f, 0.0f }, 0.3f, 10.0f);
			ConstraintParams3D params = rod(anchor, load, 2.0f, 100.0f, 0);
			params.BreakTorque = 50.0f;
			const ConstraintId3D joint = world->CreateConstraint(params);
			float force = -1.0f, torque = -1.0f;
			Check(world->GetConstraintBreakLimits(joint, force, torque) && force == 100.0f && torque == 50.0f, "limits: the creation params round-trip");
			world->SetConstraintBreakLimits(joint, 200.0f, 0.0f);
			world->GetConstraintBreakLimits(joint, force, torque);
			Check(force == 200.0f && torque == 0.0f, "limits: SetConstraintBreakLimits round-trips");
			world->SetConstraintBreakLimits(joint, -1.0f, 5.0f);
			world->SetConstraintBreakLimits(joint, std::nanf(""), 5.0f);
			world->SetConstraintBreakLimits(joint, 5.0f, std::nanf(""));
			world->GetConstraintBreakLimits(joint, force, torque);
			Check(force == 200.0f && torque == 0.0f, "limits: negative and NaN values are ignored");
			world->SetConstraintBreakLimits(joint, 0.0f, 0.0f);
			world->GetConstraintBreakLimits(joint, force, torque);
			Check(force == 0.0f && torque == 0.0f, "limits: 0 turns breaking off");

			ConstraintParams3D bad = rod(anchor, load, 2.0f, -5.0f, 0);
			bad.BreakTorque = std::nanf("");
			const ConstraintId3D badJoint = world->CreateConstraint(bad);
			world->GetConstraintBreakLimits(badJoint, force, torque);
			Check(force == 0.0f && torque == 0.0f, "limits: negative and NaN creation values become 0");

			world->DestroyConstraint(joint);
			force = torque = -1.0f;
			Check(!world->GetConstraintBreakLimits(joint, force, torque) && force == -1.0f && torque == -1.0f, "limits: a stale handle reports false and leaves the outputs");
			Check(!world->GetConstraintReaction(joint, force, torque) && force == -1.0f, "reaction: a stale handle reports false and leaves the outputs");
		}
	}

	void ConstraintTest::Initialize()
	{
		m_Checks.clear();

		RunTypeChecks();
		RunPairChecks();
		RunLifetimeChecks();
		RunSceneChecks();
		RunMotorChecks();
		RunMotorSceneChecks();
		RunGroupChecks();
		RunGroupSceneChecks();
		RunBreakChecks();

		m_BoxMesh = Mesh::CreateBox();
		m_Scene = new Scene("Constraint Test");

		m_Anchor = m_Scene->CreateEntity("Anchor");
		m_Anchor.AddComponent<Transform3DComponent>(Transform3DComponent({ 0.0f, 6.0f, 0.0f }, { 0.4f, 0.4f, 0.4f }));
		m_Anchor.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_BoxMesh, { 0.8f, 0.3f, 0.3f, 1.0f }));
		m_Anchor.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Static));

		m_Pendulum = m_Scene->CreateEntity("Pendulum");
		m_Pendulum.AddComponent<Transform3DComponent>(Transform3DComponent({ 3.0f, 6.0f, 0.0f }));
		m_Pendulum.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_BoxMesh, { 0.3f, 0.6f, 0.9f, 1.0f }));
		m_Pendulum.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Dynamic));
		Constraint3DComponent joint(ConstraintType3D::Hinge, m_Anchor.GetUUID());
		joint.AnchorA = { -3.0f, 0.0f, 0.0f };
		joint.AxisA = { 0.0f, 0.0f, 1.0f };
		joint.AxisB = { 0.0f, 0.0f, 1.0f };
		joint.NormalAxisA = { 1.0f, 0.0f, 0.0f };
		joint.NormalAxisB = { 1.0f, 0.0f, 0.0f };
		m_Pendulum.AddComponent<Constraint3DComponent>(joint);

		m_MotorJoint = m_Scene->CreateEntity("Motor Anchor");
		m_MotorJoint.AddComponent<Transform3DComponent>(Transform3DComponent({ -7.0f, 6.0f, 0.0f }, { 0.4f, 0.4f, 0.4f }));
		m_MotorJoint.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_BoxMesh, { 0.8f, 0.3f, 0.3f, 1.0f }));
		m_MotorJoint.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Static));

		m_MotorArm = m_Scene->CreateEntity("Motor Arm");
		m_MotorArm.AddComponent<Transform3DComponent>(Transform3DComponent({ -5.0f, 6.0f, 0.0f }, { 4.0f, 0.2f, 0.2f }));
		m_MotorArm.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_BoxMesh, { 0.3f, 0.8f, 0.4f, 1.0f }));
		m_MotorArm.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Dynamic));
		Constraint3DComponent motorJoint(ConstraintType3D::Hinge, m_MotorArm.GetUUID());
		motorJoint.AnchorB = { -2.0f, 0.0f, 0.0f };
		motorJoint.AxisA = motorJoint.AxisB = { 0.0f, 0.0f, 1.0f };
		motorJoint.MotorMode = MotorMode3D::Position;
		motorJoint.Motor = { 30000.0f, 3000.0f, 6000.0f };
		motorJoint.TargetRotation = glm::angleAxis(0.8f, glm::vec3(0.0f, 0.0f, 1.0f));
		m_MotorJoint.AddComponent<Constraint3DComponent>(motorJoint);

		m_Scene->OnStart();

		m_Camera = PerspectiveCamera(45.0f, m_AspectRatio, 0.1f, 200.0f);
		m_Camera.SetPosition({ 0.0f, 3.0f, 14.0f });
		m_Camera.SetTarget({ 0.0f, 3.0f, 0.0f });
	}

	void ConstraintTest::Update(float deltaTime)
	{
		m_Scene->OnUpdate((std::min)(deltaTime, 1.0f / 30.0f));

		Renderer3D& renderer = Application::Get().GetRenderer3D();
		renderer.BeginScene(m_Camera);
		renderer.Clear(m_ClearColor);
		m_Scene->SubmitLights(renderer);
		m_Scene->RenderEntities3D(renderer);
		renderer.EndScene();
	}

	void ConstraintTest::Cleanup()
	{
		delete m_Scene;
		m_Scene = nullptr;
		m_Pendulum = {};
		m_Anchor = {};
		m_MotorJoint = {};
		m_MotorArm = {};
		delete m_BoxMesh;
		m_BoxMesh = nullptr;
	}

	void ConstraintTest::Resize(uint32_t width, uint32_t height)
	{
		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
		m_Camera.SetAspectRatio(m_AspectRatio);
	}

	void ConstraintTest::ImGuiRender()
	{
		GraphicsTest::ImGuiRender();
		ImGui::Separator();
		if (ImGui::Button("Reset"))
		{
			Cleanup();
			Initialize();
			return;
		}

		if (m_MotorJoint.IsValid() && m_MotorJoint.HasComponent<Constraint3DComponent>())
		{
			Constraint3DComponent& joint = m_MotorJoint.GetComponent<Constraint3DComponent>();
			ImGui::Separator();
			ImGui::TextUnformatted("Motor arm");
			const char* modes[] = { "Off", "Velocity", "Position" };
			int mode = static_cast<int>(joint.MotorMode);
			if (ImGui::Combo("Mode", &mode, modes, 3))
				joint.MotorMode = static_cast<MotorMode3D>(mode);
			ImGui::SliderFloat("Stiffness", &joint.Motor.Stiffness, 0.0f, 100000.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("Damping", &joint.Motor.Damping, 0.0f, 10000.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("MaxTorque", &joint.Motor.MaxTorque, 0.0f, 20000.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
			float target = AngleOf(joint.TargetRotation);
			if (ImGui::SliderAngle("Target angle", &target, -90.0f, 90.0f))
				joint.TargetRotation = AboutZ(target);
			float velocity = joint.TargetAngularVelocity.z;
			if (ImGui::SliderFloat("Target velocity", &velocity, -5.0f, 5.0f))
				joint.TargetAngularVelocity = { 0.0f, 0.0f, velocity };
			ImGui::Text("Current angle %.1f deg", glm::degrees(AngleOf(m_MotorArm.GetComponent<Transform3DComponent>().Rotation)));
		}

		ImGui::Separator();
		for (const CheckResult& check : m_Checks)
		{
			const ImVec4 color = check.Passed ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f) : ImVec4(0.95f, 0.3f, 0.3f, 1.0f);
			ImGui::TextColored(color, "%s %s", check.Passed ? "[PASS]" : "[FAIL]", check.Name.c_str());
		}
	}

}
