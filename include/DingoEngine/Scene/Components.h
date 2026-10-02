#pragma once

#include "DingoEngine/Core/UUID.h"
#include "DingoEngine/Graphics/Texture.h"
#include "DingoEngine/Graphics/Font.h"
#include "DingoEngine/Graphics/Mesh.h"
#include "DingoEngine/Graphics/Light.h"
#include "DingoEngine/Physics/2D/PhysicsTypes2D.h"
#include "DingoEngine/Physics/3D/PhysicsTypes3D.h"
#include "DingoEngine/Audio/AudioTypes.h"
#include "DingoEngine/Audio/AudioEngine.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace Dingo
{

	class Material; // referenced by MeshRendererComponent (pointer only)
	class Model;    // referenced by SkinnedMeshRendererComponent (pointer only)
	struct Transform3DComponent; // the light components' ToLight, defined after it

	// Identity ----------------------------------------------------------------

	struct IDComponent
	{
		UUID ID;

		IDComponent() = default;
		IDComponent(const IDComponent&) = default;
		IDComponent(UUID id) : ID(id) {}
	};

	struct TagComponent
	{
		std::string Tag;

		TagComponent() = default;
		TagComponent(const TagComponent&) = default;
		TagComponent(const std::string& tag) : Tag(tag) {}
	};

	// Spatial -----------------------------------------------------------------

	// 2D-oriented transform. Position is the center of the entity (matching the
	// Renderer2D quad convention); Size is the full extent in world units; Rotation
	// is in degrees about the +Z axis. Position and Rotation are relative to the
	// entity's parent when it has one (Entity::SetParent); Size never is.
	struct TransformComponent
	{
		glm::vec3 Position{ 0.0f };
		float Rotation = 0.0f;
		glm::vec2 Size{ 1.0f };

		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;
		TransformComponent(const glm::vec3& position, const glm::vec2& size = glm::vec2(1.0f))
			: Position(position), Size(size) {}

		glm::mat4 GetTransform() const
		{
			glm::mat4 transform = glm::translate(glm::mat4(1.0f), Position);
			if (Rotation != 0.0f)
				transform *= glm::rotate(glm::mat4(1.0f), glm::radians(Rotation), glm::vec3(0.0f, 0.0f, 1.0f));
			transform *= glm::scale(glm::mat4(1.0f), glm::vec3(Size, 1.0f));
			return transform;
		}
	};

	// Rendering ---------------------------------------------------------------

	struct SpriteRendererComponent
	{
		glm::vec4 Color{ 1.0f };
		Texture* Texture = nullptr; // optional; null draws a solid-colour quad

		SpriteRendererComponent() = default;
		SpriteRendererComponent(const SpriteRendererComponent&) = default;
		SpriteRendererComponent(const glm::vec4& color) : Color(color) {}
	};

	struct CircleRendererComponent
	{
		glm::vec4 Color{ 1.0f };
		float Thickness = 1.0f;
		float Fade = 0.005f;

		CircleRendererComponent() = default;
		CircleRendererComponent(const CircleRendererComponent&) = default;
	};

	struct TextComponent
	{
		std::string Text;
		Font* Font = nullptr;
		glm::vec4 Color{ 1.0f };
		float Size = 1.0f;
		bool Centered = false; // when true, the text is horizontally centered on Position

		TextComponent() = default;
		TextComponent(const TextComponent&) = default;
	};

	// Camera ------------------------------------------------------------------

	// The scene's camera, read by the SceneRenderer. The projection is computed
	// from these fields (the viewport aspect is supplied by the renderer); the VIEW
	// comes from the camera entity's transform — an Orthographic camera uses the
	// entity's 2D TransformComponent, a Perspective camera its Transform3DComponent.
	// Mark exactly one camera Primary per scene.
	struct CameraComponent
	{
		enum class ProjectionType { Orthographic, Perspective };

		ProjectionType Type = ProjectionType::Orthographic;

		// Orthographic: full visible height in world units (width follows aspect).
		float OrthographicSize = 10.0f;
		float OrthoNear = -1.0f;
		float OrthoFar = 1.0f;

		// Perspective: vertical field of view in degrees.
		float FOV = 45.0f;
		float PerspNear = 0.1f;
		float PerspFar = 1000.0f;

		bool Primary = true;

		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;

		// Projection for the given viewport aspect (width / height). Uses the same glm
		// calls as the rest of the engine, so the depth convention matches.
		glm::mat4 GetProjection(float aspect) const
		{
			if (Type == ProjectionType::Perspective)
				return glm::perspective(glm::radians(FOV), aspect, PerspNear, PerspFar);

			const float halfHeight = OrthographicSize * 0.5f;
			const float halfWidth = halfHeight * aspect;
			return glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, OrthoNear, OrthoFar);
		}
	};

	// Lighting ----------------------------------------------------------------
	//
	// The SceneRenderer submits these to Renderer3D every frame (Scene::SubmitLights); see
	// Graphics/Light.h for falloff and the per-scene light budget. A scene without a single light
	// component is lit by a default DirectionalLightComponent, so a 3D scene never renders black
	// by accident. Any light component, even a disabled one, turns that default off. Point and
	// spot lights take their position, and a spot its aim, from the entity's world transform
	// (its Transform3DComponent through any parents), and are ignored without one.

	// A sun-like light. The defaults reproduce the engine's original lighting.
	struct DirectionalLightComponent
	{
		glm::vec3 Direction{ -0.4f, -1.0f, -0.35f }; // the way the light travels
		glm::vec3 Color{ 1.0f };
		float Intensity = 1.0f;
		// The engine's original single knob: white ambient this light adds to the scene, with
		// the light itself scaled by (1 - Ambient), so a face turned squarely to it gets
		// Ambient + Intensity * (1 - Ambient): full brightness at Intensity 1. Every
		// DirectionalLightComponent adds its own. Set it to 0 to light the scene with
		// AmbientLightComponent instead and get Intensity unscaled.
		float Ambient = 0.35f;

		DirectionalLightComponent() = default;
		DirectionalLightComponent(const DirectionalLightComponent&) = default;
	};

	// Light that reaches every face equally. Every ambient source in a scene adds up.
	struct AmbientLightComponent
	{
		glm::vec3 Color{ 1.0f };
		float Intensity = 0.1f;

		AmbientLightComponent() = default;
		AmbientLightComponent(const AmbientLightComponent&) = default;
		AmbientLightComponent(const glm::vec3& color, float intensity)
			: Color(color), Intensity(intensity) {}
	};

	// Light from the entity's position in every direction, reaching zero at Range.
	struct PointLightComponent
	{
		glm::vec3 Color{ 1.0f };
		float Intensity = 1.0f;
		float Range = 10.0f;
		bool Enabled = true;

		PointLightComponent() = default;
		PointLightComponent(const PointLightComponent&) = default;
		PointLightComponent(const glm::vec3& color, float intensity, float range)
			: Color(color), Intensity(intensity), Range(range) {}

		// The light Scene::SubmitLights draws for this component, placed at the transform's
		// position. Enabled is not consulted. The transform is in world space: under a parent, pass
		// the entity's GetWorldPosition/GetWorldRotation rather than its local component.
		PointLight ToLight(const Transform3DComponent& transform) const;
	};

	// A cone of light from the entity's position, reaching zero at Range. Direction is in the
	// entity's local space, so rotating the entity aims the cone; by default it points along the
	// entity's forward axis.
	struct SpotLightComponent
	{
		glm::vec3 Color{ 1.0f };
		float Intensity = 1.0f;
		float Range = 10.0f;
		float InnerConeAngle = 20.0f; // degrees from the axis at full strength
		float OuterConeAngle = 30.0f; // degrees from the axis where it reaches zero
		glm::vec3 Direction{ 0.0f, 0.0f, -1.0f };
		bool Enabled = true;

		SpotLightComponent() = default;
		SpotLightComponent(const SpotLightComponent&) = default;
		SpotLightComponent(const glm::vec3& color, float intensity, float range)
			: Color(color), Intensity(intensity), Range(range) {}

		// The light Scene::SubmitLights draws for this component: the transform's position, aimed
		// along Rotation * Direction. Enabled is not consulted. The transform is in world space, as
		// for PointLightComponent::ToLight.
		SpotLight ToLight(const Transform3DComponent& transform) const;
	};

	// Physics -----------------------------------------------------------------

	// A 2D rigid body. The simulating body lives in the Scene's Physics2D world
	// (a Box2D backend, kept entirely inside the engine); Scene::GetRuntimeBody2D
	// returns an opaque handle to it while the scene's physics is running. No backend
	// type ever appears in this public header.
	struct RigidBody2DComponent
	{
		// Alias the backend-agnostic physics enum so existing references such as
		// RigidBody2DComponent::BodyType::Dynamic keep working.
		using BodyType = BodyType2D;

		BodyType Type = BodyType::Static;
		bool FixedRotation = false; // lock rotation about Z (e.g. a player character)

		RigidBody2DComponent() = default;
		RigidBody2DComponent(const RigidBody2DComponent&) = default;
		RigidBody2DComponent(BodyType type) : Type(type) {}
	};

	// A box collision shape for an entity that also has a RigidBody2DComponent.
	// Size is the half-extent expressed as a fraction of TransformComponent::Size,
	// so the default { 0.5, 0.5 } exactly covers the entity's quad. Offset is in
	// the same fractional units, relative to the entity center.
	struct BoxCollider2DComponent
	{
		glm::vec2 Offset{ 0.0f };
		glm::vec2 Size{ 0.5f };

		float Density = 1.0f;
		float Friction = 0.5f;
		float Restitution = 0.0f;

		BoxCollider2DComponent() = default;
		BoxCollider2DComponent(const BoxCollider2DComponent&) = default;
	};

	// A circle collision shape. Radius is a fraction of TransformComponent::Size.x,
	// so the default 0.5 inscribes the entity's quad.
	struct CircleCollider2DComponent
	{
		glm::vec2 Offset{ 0.0f };
		float Radius = 0.5f;

		float Density = 1.0f;
		float Friction = 0.5f;
		float Restitution = 0.0f;

		CircleCollider2DComponent() = default;
		CircleCollider2DComponent(const CircleCollider2DComponent&) = default;
	};

	// 3D spatial / rendering / physics ----------------------------------------
	//
	// These are the 3D counterparts to the 2D components above. A 3D entity uses a
	// Transform3DComponent (the default TransformComponent it receives on creation
	// is 2D and simply goes unused); it is rendered through Renderer3D when it also
	// has a MeshRendererComponent, and simulated in the Scene's Physics3D world when
	// it has a RigidBody3DComponent plus a collider.

	// 3D transform. Position is the entity center; Rotation is a quaternion; Scale
	// is the full extent multiplier per axis. All three are relative to the entity's parent
	// when it has one (Entity::SetParent); Entity::GetWorldTransform gives the world values.
	// The Scene writes the simulated position/rotation back here each frame while 3D
	// physics is running.
	struct Transform3DComponent
	{
		glm::vec3 Position{ 0.0f };
		glm::quat Rotation{ 1.0f, 0.0f, 0.0f, 0.0f }; // identity (w, x, y, z)
		glm::vec3 Scale{ 1.0f };

		Transform3DComponent() = default;
		Transform3DComponent(const Transform3DComponent&) = default;
		Transform3DComponent(const glm::vec3& position, const glm::vec3& scale = glm::vec3(1.0f))
			: Position(position), Scale(scale) {}

		glm::mat4 GetTransform() const
		{
			return glm::translate(glm::mat4(1.0f), Position)
				* glm::mat4_cast(Rotation)
				* glm::scale(glm::mat4(1.0f), Scale);
		}

		// Authoring convenience: set the rotation from XYZ Euler angles in degrees.
		void SetRotationEuler(const glm::vec3& eulerDegrees)
		{
			Rotation = glm::quat(glm::radians(eulerDegrees));
		}

		// Engine-wide camera/listener convention: local -Z is forward, +Y is up.
		glm::vec3 Forward() const { return Rotation * glm::vec3(0.0f, 0.0f, -1.0f); }
		glm::vec3 Up() const { return Rotation * glm::vec3(0.0f, 1.0f, 0.0f); }
	};

	inline PointLight PointLightComponent::ToLight(const Transform3DComponent& transform) const
	{
		return PointLight{ .Position = transform.Position, .Color = Color, .Intensity = Intensity, .Range = Range };
	}

	inline SpotLight SpotLightComponent::ToLight(const Transform3DComponent& transform) const
	{
		return SpotLight{ .Position = transform.Position, .Direction = transform.Rotation * Direction, .Color = Color,
			.Intensity = Intensity, .Range = Range, .InnerConeAngle = InnerConeAngle, .OuterConeAngle = OuterConeAngle };
	}

	// A renderable mesh drawn by Renderer3D at the entity's Transform3D, tinted by
	// Color. The mesh is not owned by the component (the game/asset system owns it),
	// exactly like SpriteRendererComponent's Texture.
	struct MeshRendererComponent
	{
		Mesh* Mesh = nullptr;
		glm::vec4 Color{ 1.0f };

		// When false, the SceneRenderer skips this entity — cheap per-entity culling that
		// replaces the old "set Mesh = nullptr and restore it later" juggling. Visible by default.
		bool Visible = true;

		// Optional material (custom shader + uniforms + textures). Null draws with
		// Renderer3D's built-in lit material. The Color above is written
		// into the vertex stream either way. Owned by the client, not the component.
		Material* Material = nullptr;

		MeshRendererComponent() = default;
		MeshRendererComponent(const MeshRendererComponent&) = default;
		MeshRendererComponent(Dingo::Mesh* mesh, const glm::vec4& color = glm::vec4(1.0f))
			: Mesh(mesh), Color(color) {}
	};

	// Draws every submesh of a Model at the entity's world transform: skinned submeshes on the GPU
	// (Renderer3D::SubmitSkinnedMesh) in the skeleton's rest pose, the rest like a
	// MeshRendererComponent. The Model is not owned by the component. Material works as on
	// MeshRendererComponent and applies to every submesh; the submeshes' own diffuse textures are
	// not used.
	struct SkinnedMeshRendererComponent
	{
		Dingo::Model* Model = nullptr;
		glm::vec4 Color{ 1.0f };
		Dingo::Material* Material = nullptr;
		bool Visible = true;

		SkinnedMeshRendererComponent() = default;
		SkinnedMeshRendererComponent(const SkinnedMeshRendererComponent&) = default;
		SkinnedMeshRendererComponent(Dingo::Model* model, const glm::vec4& color = glm::vec4(1.0f))
			: Model(model), Color(color) {}
	};

	// Settings for the entity's Animator, which poses its SkinnedMeshRendererComponent::Model;
	// Scene::GetAnimator returns it for playing clips from a script. Scene::OnUpdate advances it
	// after the scripts and before physics, by deltaTime x Speed while Enabled.
	struct AnimatorComponent
	{
		// Played, looping, when the animator is created (or its model changes) if PlayOnStart is set.
		std::string DefaultClip;
		bool PlayOnStart = true;
		float Speed = 1.0f;
		// False holds the current pose.
		bool Enabled = true;

		AnimatorComponent() = default;
		AnimatorComponent(const AnimatorComponent&) = default;
		AnimatorComponent(std::string defaultClip) : DefaultClip(std::move(defaultClip)) {}
	};

	// A 3D rigid body simulated in the Scene's Physics3D world (Jolt backend, hidden
	// behind the Physics3D interface). Scene::GetRuntimeBody3D returns an opaque handle
	// to it while the scene's physics is running. Unlike the 2D collider components, the 3D
	// collider shape is baked into the body when it is created, so a 3D rigid-body
	// entity needs exactly one Box/Sphere/Capsule/MeshCollider3DComponent alongside this.
	struct RigidBody3DComponent
	{
		// Alias the backend-agnostic enum so RigidBody3DComponent::BodyType::Dynamic works.
		using BodyType = BodyType3D;

		BodyType Type = BodyType::Static;

		// See RigidBodyParams3D::ContinuousCollision: turn on for fast bodies that must not
		// tunnel through MeshCollider3DComponent geometry.
		bool ContinuousCollision = false;

		RigidBody3DComponent() = default;
		RigidBody3DComponent(const RigidBody3DComponent&) = default;
		RigidBody3DComponent(BodyType type) : Type(type) {}
	};

	// A box collider for an entity with a RigidBody3DComponent. HalfExtents is a
	// fraction of the entity's world scale, so the default { 0.5, 0.5, 0.5 }
	// exactly covers the entity's box. (Physics3D centers the shape on the body, so
	// there is no per-collider offset — model offset with the Transform instead.)
	struct BoxCollider3DComponent
	{
		glm::vec3 HalfExtents{ 0.5f };

		float Friction = 0.5f;
		float Restitution = 0.0f;

		BoxCollider3DComponent() = default;
		BoxCollider3DComponent(const BoxCollider3DComponent&) = default;
	};

	// A sphere collider. Radius is a fraction of the entity's world scale x, so
	// the default 0.5 inscribes a unit box.
	struct SphereCollider3DComponent
	{
		float Radius = 0.5f;

		float Friction = 0.5f;
		float Restitution = 0.0f;

		SphereCollider3DComponent() = default;
		SphereCollider3DComponent(const SphereCollider3DComponent&) = default;
	};

	// A capsule collider for an entity with a RigidBody3DComponent. The capsule stands
	// on the +Y axis. Radius is a fraction of the entity's world scale x and
	// HalfHeight (half the cylinder section between the caps) is a fraction of
	// its world scale y, so on a unit-scaled entity the defaults give a
	// 1-unit-tall capsule of radius 0.5.
	struct CapsuleCollider3DComponent
	{
		float Radius = 0.5f;
		float HalfHeight = 0.5f;

		float Friction = 0.5f;
		float Restitution = 0.0f;

		CapsuleCollider3DComponent() = default;
		CapsuleCollider3DComponent(const CapsuleCollider3DComponent&) = default;
	};

	// A collider shaped like a Mesh, for an entity with a RigidBody3DComponent. The mesh
	// is scaled by the entity's world scale exactly as MeshRendererComponent draws it,
	// so the collider matches what is on screen. A null Mesh uses the entity's
	// MeshRendererComponent::Mesh. Not owned, and only read when the body is built.
	//
	// Convex = false collides against the triangles themselves, for level geometry:
	// Static or Kinematic bodies only. Convex = true uses the convex hull of the vertices
	// and works for any body type; a Dynamic body always gets the hull.
	struct MeshCollider3DComponent
	{
		Dingo::Mesh* Mesh = nullptr;
		bool Convex = false;

		float Friction = 0.5f;
		float Restitution = 0.0f;

		MeshCollider3DComponent() = default;
		MeshCollider3DComponent(const MeshCollider3DComponent&) = default;
		MeshCollider3DComponent(Dingo::Mesh* mesh, bool convex = false)
			: Mesh(mesh), Convex(convex) {}
	};

	// A kinematic character controller for player/enemy movement, wrapping Jolt's
	// CharacterVirtual behind the Scene's Physics3D. The Scene creates the controller
	// at OnPhysicsStart from the params below (positioned at the entity's Transform3D),
	// steps it each physics update, and writes its position back onto the
	// Transform3DComponent. Scripts steer it via Scene::GetCharacterController(entity)
	// (set velocity, read IsGrounded, etc.). An entity should have EITHER this OR a
	// RigidBody3DComponent, not both.
	struct CharacterController3DComponent
	{
		float Radius = 0.3f;         // capsule radius
		float Height = 1.8f;         // full standing height
		float StepHeight = 0.3f;     // max stair step-up height
		float MaxSlopeAngle = 45.0f; // steepest walkable slope, degrees

		CharacterController3DComponent() = default;
		CharacterController3DComponent(const CharacterController3DComponent&) = default;
	};

	// Audio ---------------------------------------------------------------------

	// A sound emitter attached to an entity. Clip is a shareable decoded-audio
	// template loaded via Application::Get().GetAudioEngine().LoadClip() — the
	// component does not own decoding, only a reference, exactly like
	// MeshRendererComponent's Mesh. When Spatialized is true the Scene keeps the
	// live sound's position in sync with the entity's transform every frame
	// (its world position if it has a Transform3DComponent, else the 2D TransformComponent at z = 0).
	struct AudioSourceComponent
	{
		std::shared_ptr<AudioClip> Clip;

		float Volume = 1.0f;
		float Pitch = 1.0f;
		bool Looping = false;
		bool Spatialized = true;
		// nullopt = use the engine's current default (AudioEngine::GetDefaultAttenuation).
		// Ignored when Spatialized is false.
		std::optional<SoundAttenuation> Attenuation;
		bool PlayOnStart = false;

		AudioSourceComponent() = default;
		AudioSourceComponent(const AudioSourceComponent&) = default;
	};

	// Marks an entity as the scene's audio listener (ears for spatialized sound).
	// Mirrors CameraComponent::Primary: the Scene uses the first Primary listener
	// it finds, or the first listener at all if none is marked Primary. Position
	// comes from the entity's transform; orientation only if it has a
	// Transform3DComponent (a 2D listener stays at the engine's default
	// orientation). If a scene has no listener entity, the AudioEngine's listener
	// is left untouched.
	struct AudioListenerComponent
	{
		bool Primary = true;

		AudioListenerComponent() = default;
		AudioListenerComponent(const AudioListenerComponent&) = default;
	};

}
