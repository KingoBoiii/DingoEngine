#pragma once

#include "DingoEngine/Core/UUID.h"
#include "DingoEngine/Graphics/Texture.h"
#include "DingoEngine/Graphics/Font.h"
#include "DingoEngine/Graphics/Mesh.h"
#include "DingoEngine/Physics/2D/PhysicsTypes2D.h"
#include "DingoEngine/Physics/3D/PhysicsTypes3D.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <string>

namespace Dingo
{

	class Material; // referenced by MeshRendererComponent (pointer only)

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
	// is in degrees about the +Z axis.
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

	// A single directional light read by the SceneRenderer and fed to Renderer3D.
	// Defaults match Renderer3D's built-in light, so a default-constructed one
	// reproduces the engine's out-of-the-box 3D lighting.
	struct DirectionalLightComponent
	{
		glm::vec3 Direction{ -0.4f, -1.0f, -0.35f }; // the way the light travels
		float Ambient = 0.35f;                        // lifts unlit faces

		DirectionalLightComponent() = default;
		DirectionalLightComponent(const DirectionalLightComponent&) = default;
	};

	// Physics -----------------------------------------------------------------

	// A 2D rigid body. The simulating body lives in the Scene's Physics2D world
	// (a Box2D backend, kept entirely inside the engine); RuntimeBody is an opaque
	// handle to it, valid only while the scene's physics is running. No backend
	// type ever appears in this public header.
	struct RigidBody2DComponent
	{
		// Alias the backend-agnostic physics enum so existing references such as
		// RigidBody2DComponent::BodyType::Dynamic keep working.
		using BodyType = BodyType2D;

		BodyType Type = BodyType::Static;
		bool FixedRotation = false; // lock rotation about Z (e.g. a player character)

		// Opaque handle to the simulated body; 0 when none.
		// NOTE: this is a live backend handle. Any future entity-duplicate / clone path
		// MUST reset it to 0 on the copy — otherwise both entities alias (and
		// DestroyEntity double-frees) the same physics body.
		PhysicsBodyId2D RuntimeBody = 0;

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

		// Opaque handle to the simulated collision shape; 0 when none.
		// NOTE: live backend handle — a future entity-duplicate path MUST reset it to 0
		// on the copy to avoid aliasing / double-freeing the same shape.
		PhysicsShapeId2D RuntimeShape = 0;

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

		// Opaque handle to the simulated collision shape; 0 when none.
		// NOTE: live backend handle — a future entity-duplicate path MUST reset it to 0
		// on the copy to avoid aliasing / double-freeing the same shape.
		PhysicsShapeId2D RuntimeShape = 0;

		CircleCollider2DComponent() = default;
		CircleCollider2DComponent(const CircleCollider2DComponent&) = default;
	};

	// 3D spatial / rendering / physics ----------------------------------------
	//
	// These are the 3D counterparts to the 2D components above. A 3D entity uses a
	// Transform3DComponent (the default TransformComponent it receives on creation
	// is 2D and simply goes unused); it is rendered through Renderer3D when it also
	// has a MeshRendererComponent, and simulated in the Scene's Physics3D world when
	// it has a RigidBody3DComponent plus a box/sphere collider.

	// 3D transform. Position is the entity center; Rotation is a quaternion; Scale
	// is the full extent multiplier per axis. The Scene writes the simulated
	// position/rotation back here each frame while 3D physics is running.
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
	};

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
		// Renderer3D's built-in flat directional-lit material. The Color above is written
		// into the vertex stream either way. Owned by the client, not the component.
		Material* Material = nullptr;

		MeshRendererComponent() = default;
		MeshRendererComponent(const MeshRendererComponent&) = default;
		MeshRendererComponent(Dingo::Mesh* mesh, const glm::vec4& color = glm::vec4(1.0f))
			: Mesh(mesh), Color(color) {}
	};

	// A 3D rigid body simulated in the Scene's Physics3D world (Jolt backend, hidden
	// behind the Physics3D interface). RuntimeBody is an opaque handle, valid only
	// while the scene's physics is running. Unlike the 2D collider components, the 3D
	// collider shape is baked into the body when it is created, so a 3D rigid-body
	// entity needs exactly one Box/SphereCollider3DComponent alongside this.
	struct RigidBody3DComponent
	{
		// Alias the backend-agnostic enum so RigidBody3DComponent::BodyType::Dynamic works.
		using BodyType = BodyType3D;

		BodyType Type = BodyType::Static;

		// Opaque handle to the simulated body; k_InvalidBody3D when none.
		// NOTE: this is a live backend handle. Any future entity-duplicate / clone path
		// MUST reset it to k_InvalidBody3D on the copy — otherwise both entities alias
		// (and DestroyEntity double-frees) the same physics body.
		PhysicsBodyId3D RuntimeBody = k_InvalidBody3D;

		RigidBody3DComponent() = default;
		RigidBody3DComponent(const RigidBody3DComponent&) = default;
		RigidBody3DComponent(BodyType type) : Type(type) {}
	};

	// A box collider for an entity with a RigidBody3DComponent. HalfExtents is a
	// fraction of Transform3DComponent::Scale, so the default { 0.5, 0.5, 0.5 }
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

	// A sphere collider. Radius is a fraction of Transform3DComponent::Scale.x, so
	// the default 0.5 inscribes a unit box.
	struct SphereCollider3DComponent
	{
		float Radius = 0.5f;

		float Friction = 0.5f;
		float Restitution = 0.0f;

		SphereCollider3DComponent() = default;
		SphereCollider3DComponent(const SphereCollider3DComponent&) = default;
	};

}
