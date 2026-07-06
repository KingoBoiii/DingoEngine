#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Dingo
{

	// Configuration for a CharacterController3D. The collision shape is a capsule
	// standing on the +Y axis: Radius is the capsule radius and Height is the full
	// standing height (the cylinder section is Height - 2*Radius, so Height must be
	// >= 2*Radius). The controller's Position refers to the bottom of the capsule
	// (its feet), matching Jolt's convention.
	struct CharacterControllerParams3D
	{
		float Radius = 0.3f;  // capsule radius
		float Height = 1.8f;  // full standing height (feet to head)

		float MaxSlopeAngle = 45.0f; // steepest walkable slope, degrees
		float StepHeight = 0.3f;     // max step-up height for stairs (Jolt walk-stairs)
		float Mass = 70.0f;          // used to push dynamic bodies the character stands on

		glm::vec3 Up{ 0.0f, 1.0f, 0.0f }; // world up axis

		glm::vec3 Position{ 0.0f };
		glm::quat Rotation{ 1.0f, 0.0f, 0.0f, 0.0f }; // identity (w, x, y, z)
	};

	// A kinematic character controller for player/enemy movement, wrapping Jolt's
	// CharacterVirtual. Unlike a dynamic rigid body it does not tumble or get shoved
	// by the solver; it moves by sweeping its capsule through the world each Update,
	// sliding along walls, snapping to the floor and stepping up stairs. It is created
	// from a live world via Physics3D::CreateCharacterController and must be destroyed
	// before that world.
	//
	// Drive contract (the caller applies gravity — this matches Jolt's ExtendedUpdate):
	// each frame set the desired velocity, then Update, then read the new position:
	//
	//     glm::vec3 v = controller->GetLinearVelocity();
	//     if (controller->IsGrounded())
	//         v.y = 0.0f;                  // reset fall speed while on the floor
	//     else
	//         v.y += gravity.y * dt;       // integrate gravity while airborne
	//     v.x = inputDir.x * moveSpeed;    // horizontal input
	//     v.z = inputDir.z * moveSpeed;
	//     if (jumpPressed && controller->IsGrounded())
	//         v.y = jumpSpeed;
	//     controller->SetLinearVelocity(v);
	//     controller->Update(dt);
	//     transform.Position = controller->GetPosition();
	//
	// No backend (Jolt) type ever appears in this interface — it is an abstract class
	// with an opaque implementation, exactly like Physics3D.
	class CharacterController3D
	{
	public:
		CharacterController3D() = default;
		virtual ~CharacterController3D() = default;

		CharacterController3D(const CharacterController3D&) = delete;
		CharacterController3D& operator=(const CharacterController3D&) = delete;

		// Position of the character's feet (bottom of the capsule).
		virtual glm::vec3 GetPosition() const = 0;
		virtual void SetPosition(const glm::vec3& position) = 0; // teleport

		virtual glm::quat GetRotation() const = 0;
		virtual void SetRotation(const glm::quat& rotation) = 0;

		virtual glm::vec3 GetLinearVelocity() const = 0;
		virtual void SetLinearVelocity(const glm::vec3& velocity) = 0;

		// Advances the character by its current velocity for deltaTime, handling ground
		// snapping and stair walking. Set the desired velocity (including gravity — see
		// the class comment) before calling.
		virtual void Update(float deltaTime) = 0;

		// True when the character is standing on ground it can walk on.
		virtual bool IsGrounded() const = 0;

		// Surface normal / velocity of the ground the character is standing on. Normal is
		// { 0, 0, 0 } and velocity is { 0, 0, 0 } when the character is airborne. Ground
		// velocity lets a rider move with a kinematic platform.
		virtual glm::vec3 GetGroundNormal() const = 0;
		virtual glm::vec3 GetGroundVelocity() const = 0;
	};

}
