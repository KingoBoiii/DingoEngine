#include "depch.h"
#include "DingoEngine/Physics/2D/Box2D/Box2DPhysics2D.h"

namespace Dingo
{

	b2BodyType Box2DPhysics2D::ToBox2DBodyType(BodyType2D type)
	{
		switch (type)
		{
			case BodyType2D::Static:    return b2_staticBody;
			case BodyType2D::Dynamic:   return b2_dynamicBody;
			case BodyType2D::Kinematic: return b2_kinematicBody;
		}
		return b2_staticBody;
	}

	Box2DPhysics2D::~Box2DPhysics2D()
	{
		Shutdown();
	}

	void Box2DPhysics2D::Initialize(const glm::vec2& gravity)
	{
		if (b2World_IsValid(m_World))
			return; // already running

		b2WorldDef worldDef = b2DefaultWorldDef();
		worldDef.gravity = { gravity.x, gravity.y };
		m_World = b2CreateWorld(&worldDef);
	}

	void Box2DPhysics2D::Shutdown()
	{
		if (!b2World_IsValid(m_World))
			return;

		b2DestroyWorld(m_World); // also destroys all bodies + shapes
		m_World = b2_nullWorldId;
	}

	bool Box2DPhysics2D::IsValid() const
	{
		return b2World_IsValid(m_World);
	}

	void Box2DPhysics2D::Step(float deltaTime, int subStepCount)
	{
		if (b2World_IsValid(m_World))
			b2World_Step(m_World, deltaTime, subStepCount);
	}

	void Box2DPhysics2D::SetGravity(const glm::vec2& gravity)
	{
		if (b2World_IsValid(m_World))
			b2World_SetGravity(m_World, { gravity.x, gravity.y });
	}

	glm::vec2 Box2DPhysics2D::GetGravity() const
	{
		if (!b2World_IsValid(m_World))
			return glm::vec2(0.0f);

		b2Vec2 gravity = b2World_GetGravity(m_World);
		return { gravity.x, gravity.y };
	}

	PhysicsBodyId2D Box2DPhysics2D::CreateBody(const RigidBodyParams2D& params)
	{
		if (!b2World_IsValid(m_World))
			return 0;

		b2BodyDef bodyDef = b2DefaultBodyDef();
		bodyDef.type = ToBox2DBodyType(params.Type);
		bodyDef.position = b2ToPos({ params.Position.x, params.Position.y });
		bodyDef.rotation = b2MakeRot(params.Rotation);
		bodyDef.motionLocks.angularZ = params.FixedRotation;

		b2BodyId bodyId = b2CreateBody(m_World, &bodyDef);
		return b2StoreBodyId(bodyId);
	}

	void Box2DPhysics2D::DestroyBody(PhysicsBodyId2D body)
	{
		if (body == 0)
			return;

		b2BodyId bodyId = b2LoadBodyId(body);
		if (b2Body_IsValid(bodyId))
			b2DestroyBody(bodyId);
	}

	bool Box2DPhysics2D::IsBodyValid(PhysicsBodyId2D body) const
	{
		if (body == 0)
			return false;

		return b2Body_IsValid(b2LoadBodyId(body));
	}

	PhysicsShapeId2D Box2DPhysics2D::AddBoxShape(PhysicsBodyId2D body, const BoxShapeParams2D& params)
	{
		if (body == 0)
			return 0;

		b2BodyId bodyId = b2LoadBodyId(body);
		if (!b2Body_IsValid(bodyId))
			return 0;

		b2ShapeDef shapeDef = b2DefaultShapeDef();
		shapeDef.density = params.Density;
		shapeDef.material.friction = params.Friction;
		shapeDef.material.restitution = params.Restitution;

		b2Polygon box = b2MakeOffsetBox(params.HalfExtents.x, params.HalfExtents.y,
										{ params.Center.x, params.Center.y }, b2Rot_identity);

		b2ShapeId shapeId = b2CreatePolygonShape(bodyId, &shapeDef, &box);
		return b2StoreShapeId(shapeId);
	}

	PhysicsShapeId2D Box2DPhysics2D::AddCircleShape(PhysicsBodyId2D body, const CircleShapeParams2D& params)
	{
		if (body == 0)
			return 0;

		b2BodyId bodyId = b2LoadBodyId(body);
		if (!b2Body_IsValid(bodyId))
			return 0;

		b2ShapeDef shapeDef = b2DefaultShapeDef();
		shapeDef.density = params.Density;
		shapeDef.material.friction = params.Friction;
		shapeDef.material.restitution = params.Restitution;

		b2Circle circle;
		circle.center = { params.Center.x, params.Center.y };
		circle.radius = params.Radius;

		b2ShapeId shapeId = b2CreateCircleShape(bodyId, &shapeDef, &circle);
		return b2StoreShapeId(shapeId);
	}

	glm::vec2 Box2DPhysics2D::GetPosition(PhysicsBodyId2D body) const
	{
		if (body == 0)
			return glm::vec2(0.0f);

		b2BodyId bodyId = b2LoadBodyId(body);
		if (!b2Body_IsValid(bodyId))
			return glm::vec2(0.0f);

		b2Vec2 position = b2ToVec2(b2Body_GetPosition(bodyId));
		return { position.x, position.y };
	}

	float Box2DPhysics2D::GetAngle(PhysicsBodyId2D body) const
	{
		if (body == 0)
			return 0.0f;

		b2BodyId bodyId = b2LoadBodyId(body);
		if (!b2Body_IsValid(bodyId))
			return 0.0f;

		return b2Rot_GetAngle(b2Body_GetRotation(bodyId));
	}

	void Box2DPhysics2D::SetLinearVelocity(PhysicsBodyId2D body, const glm::vec2& velocity)
	{
		if (body == 0)
			return;

		b2BodyId bodyId = b2LoadBodyId(body);
		if (b2Body_IsValid(bodyId))
			b2Body_SetLinearVelocity(bodyId, { velocity.x, velocity.y });
	}

	glm::vec2 Box2DPhysics2D::GetLinearVelocity(PhysicsBodyId2D body) const
	{
		if (body == 0)
			return glm::vec2(0.0f);

		b2BodyId bodyId = b2LoadBodyId(body);
		if (!b2Body_IsValid(bodyId))
			return glm::vec2(0.0f);

		b2Vec2 velocity = b2Body_GetLinearVelocity(bodyId);
		return { velocity.x, velocity.y };
	}

	void Box2DPhysics2D::ApplyLinearImpulse(PhysicsBodyId2D body, const glm::vec2& impulse, const glm::vec2& worldPoint, bool wake)
	{
		if (body == 0)
			return;

		b2BodyId bodyId = b2LoadBodyId(body);
		if (b2Body_IsValid(bodyId))
			b2Body_ApplyLinearImpulse(bodyId, { impulse.x, impulse.y }, b2ToPos({ worldPoint.x, worldPoint.y }), wake);
	}

	void Box2DPhysics2D::ApplyLinearImpulseToCenter(PhysicsBodyId2D body, const glm::vec2& impulse, bool wake)
	{
		if (body == 0)
			return;

		b2BodyId bodyId = b2LoadBodyId(body);
		if (b2Body_IsValid(bodyId))
			b2Body_ApplyLinearImpulseToCenter(bodyId, { impulse.x, impulse.y }, wake);
	}

	void Box2DPhysics2D::ApplyForceToCenter(PhysicsBodyId2D body, const glm::vec2& force, bool wake)
	{
		if (body == 0)
			return;

		b2BodyId bodyId = b2LoadBodyId(body);
		if (b2Body_IsValid(bodyId))
			b2Body_ApplyForceToCenter(bodyId, { force.x, force.y }, wake);
	}

}
