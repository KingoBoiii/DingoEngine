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

	void ConstraintTest::Initialize()
	{
		m_Checks.clear();

		RunTypeChecks();
		RunPairChecks();
		RunLifetimeChecks();
		RunSceneChecks();

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

		ImGui::Separator();
		for (const CheckResult& check : m_Checks)
		{
			const ImVec4 color = check.Passed ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f) : ImVec4(0.95f, 0.3f, 0.3f, 1.0f);
			ImGui::TextColored(color, "%s %s", check.Passed ? "[PASS]" : "[FAIL]", check.Name.c_str());
		}
	}

}
