#pragma once

// Engine-internal: a plain-data window onto every live scene, its entity hierarchy and one
// entity's components, for the F9 debug tab. Keeps EnTT and SceneData out of the UI code. Also
// the list of live scenes the F7 tab (AnimationDebug) reads.

#include "DingoEngine/Core/UUID.h"
#include "DingoEngine/Physics/3D/PhysicsTypes3D.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace Dingo
{

	class Scene;

	namespace Internal
	{

		struct SceneData;

		namespace SceneDebug
		{

			struct LiveScene
			{
				Scene* Instance = nullptr;
				SceneData* Data = nullptr;
				// Never reused, so a selection made on a destroyed scene can't land on a new one at its address.
				uint64_t Serial = 0;
			};

			// A scene registers at its first OnUpdate (main thread) and unregisters in its destructor
			// (any thread), which waits while this lock is held. Hold it while reading LiveScenes() or
			// anything a row points into, so a scene freed on another thread can't be freed mid-read.
			// The Collect functions take it themselves.
			[[nodiscard]] std::unique_lock<std::recursive_mutex> LockScenes();
			void RegisterScene(Scene* scene, SceneData* data);
			void UnregisterScene(Scene* scene);

			// In the order scenes first updated. Only while LockScenes() is held.
			const std::vector<LiveScene>& LiveScenes();

			enum class Component : uint32_t
			{
				Transform, Transform3D,
				SpriteRenderer, CircleRenderer, Text,
				Camera, PostProcess,
				DirectionalLight, AmbientLight, PointLight, SpotLight, Fog,
				RigidBody2D, BoxCollider2D, CircleCollider2D,
				MeshRenderer, SkinnedMeshRenderer, Animator, ParticleEmitter, ParticleEvent,
				RigidBody3D, BoxCollider3D, SphereCollider3D, CapsuleCollider3D, MeshCollider3D, CharacterController3D, Constraint3D, Ragdoll,
				AudioSource, AudioListener,
				Script,
				Count
			};

			const char* ComponentName(Component component);

			constexpr uint64_t ComponentBit(Component component) { return uint64_t(1) << static_cast<uint32_t>(component); }

			struct SceneRow
			{
				uint64_t Serial = 0;
				std::string Name;
				bool Running = false;
				bool PhysicsRunning = false;
				bool Has2DWorld = false;
				bool Has3DWorld = false;
				uint32_t Entities = 0;
				uint32_t Roots = 0;
				uint32_t Scripts = 0;

				// Light components. Point and spot count only enabled ones on an active entity with a
				// Transform3DComponent; the renderer ignores the others, as UnplacedLocalLights says.
				uint32_t DirectionalLights = 0;
				uint32_t AmbientLights = 0;
				uint32_t PointLights = 0;
				uint32_t SpotLights = 0;
				uint32_t DisabledLocalLights = 0;
				// Enabled, but on an entity that is inactive or under an inactive ancestor (Entity::SetActive).
				uint32_t InactiveLocalLights = 0;
				uint32_t UnplacedLocalLights = 0;
				uint32_t ShadowedLocalLights = 0;
				// Whether any component turns the default directional light off, as LightSystem decides it.
				bool HasLight = false;

				// Live bodies and controllers, from the runtime components.
				uint32_t Bodies2D = 0;
				uint32_t Bodies3D = 0;
				uint32_t Sensors3D = 0;
				uint32_t Controllers = 0;

				// The primary camera (CameraUtils::FindPrimaryCamera); empty name without one.
				std::string Camera;
				std::string CameraProjection;

				uint32_t FogComponents = 0;
				glm::vec4 ClearColor{ 0.0f };
				glm::vec2 Gravity2D{ 0.0f };
				glm::vec3 Gravity3D{ 0.0f };
				bool FixedTick = false;
				bool InterpolatePhysics = false;
				float FixedStep = 0.0f;
				float FixedAlpha = 0.0f;
				uint64_t Tick = 0;
				uint32_t FixedStepsLastUpdate = 0;
				uint64_t DroppedFixedSteps = 0;
			};

			void CollectScenes(std::vector<SceneRow>& out);

			struct EntityRow
			{
				UUID Id;
				// The entity's slot, which tells apart two entities given the same UUID.
				uint32_t Index = 0;
				std::string Name;
				uint32_t Depth = 0;
				uint32_t ChildCount = 0;
				std::string Socket;
				uint64_t Components = 0; // ComponentBit of each it carries
			};

			// The scene's entities depth first: each root, in entity index order (creation order, but for a
			// recycled slot, which sorts where the old entity was), followed by its subtree in child order.
			// False (out empty) when no live scene has this serial.
			bool CollectEntities(uint64_t sceneSerial, std::vector<EntityRow>& out);

			struct Field
			{
				std::string Name;
				std::string Value;
			};

			struct Section
			{
				std::string Title;
				// Read from RuntimeComponents.h (a live body, controller, sound, animator, emitter or script),
				// not from a settings component.
				bool Runtime = false;
				std::vector<Field> Fields;
			};

			// The motor of a Hinge or SwingTwist Constraint3DComponent: the live settings the F9 tab edits
			// (WriteMotor) and what the joint is doing. Angles are about Axis (the component's AxisA).
			struct MotorView
			{
				bool Present = false;
				bool SwingTwist = false;
				// A built joint exists, so Angle and the torques mean something.
				bool Live = false;
				MotorMode3D Mode = MotorMode3D::Off;
				MotorParams3D Motor;
				MotorParams3D TwistMotor;
				glm::vec3 Axis{ 0.0f, 1.0f, 0.0f };
				float TargetAngle = 0.0f;
				float TargetVelocity = 0.0f;
				float Angle = 0.0f;
				float Torque = 0.0f;
				float TwistTorque = 0.0f;
			};

			// A RagdollComponent: the live settings the F9 tab edits (WriteRagdoll) and each built body.
			struct RagdollView
			{
				bool Present = false;
				// Built bodies exist, so Parts means something.
				bool Live = false;
				float Blend = 1.0f;
				float Strength = 1.0f;
				bool DriveToAnimation = true;
				float TotalMass = 0.0f;
				bool CollideWithinRagdoll = false;
				std::string Recipe;
				// The entity's position follows the root body (limp, nothing else moving it).
				bool FollowsRoot = false;

				struct Part
				{
					std::string Bone;
					// Empty for the root body.
					std::string Parent;
					bool Hinge = false;
					float Mass = 0.0f;
					float Speed = 0.0f;
					// The motor's torque cap at the last step (0 while limp) and the torque it applied.
					float MaxTorque = 0.0f;
					float Torque = 0.0f;
					float TwistTorque = 0.0f;
				};
				std::vector<Part> Parts;
			};

			struct EntityDetail
			{
				UUID Id;
				std::string Name;
				// Entity::IsActive and IsActiveInHierarchy.
				bool Active = true;
				bool ActiveInHierarchy = true;
				bool HasParent = false;
				UUID Parent;
				std::string ParentName;
				uint32_t ChildCount = 0;
				std::string Socket;

				// Through HierarchySystem, so a child's are its world values, not its component's.
				bool Has3D = false;
				glm::vec3 WorldPosition{ 0.0f };
				glm::quat WorldRotation{ 1.0f, 0.0f, 0.0f, 0.0f };
				glm::vec3 WorldScale{ 1.0f };

				bool Has2D = false;
				glm::vec3 WorldPosition2D{ 0.0f };
				float WorldRotation2D = 0.0f;

				std::vector<Section> Sections;
				MotorView Motor;
				RagdollView Ragdoll;
			};

			// False (out reset) when the scene or the entity is gone.
			bool CollectEntity(uint64_t sceneSerial, UUID id, EntityDetail& out);

			// Writes the motor fields of the entity's Constraint3DComponent from `motor`, under the scenes lock
			// CollectEntity takes; the scene applies them at its next physics step. False when the scene or
			// the entity is gone or has no Hinge or SwingTwist constraint.
			bool WriteMotor(uint64_t sceneSerial, UUID id, const MotorView& motor);

			// Writes Blend, Strength and DriveToAnimation of the entity's RagdollComponent from `ragdoll`,
			// under the scenes lock; the scene reads them at its next step and pose. False when the scene
			// or the entity is gone or has no RagdollComponent.
			bool WriteRagdoll(uint64_t sceneSerial, UUID id, const RagdollView& ragdoll);

		}

	}

}
