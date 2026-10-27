#pragma once

#include "DingoEngine/Common.h"
#include "DingoEngine/Core/UUID.h"
#include "DingoEngine/Graphics/Texture.h"
#include "DingoEngine/Graphics/Font.h"
#include "DingoEngine/Graphics/Mesh.h"
#include "DingoEngine/Graphics/Light.h"
#include "DingoEngine/Graphics/SurfaceOverride.h"
#include "DingoEngine/Graphics/PostProcess.h"
#include "DingoEngine/Graphics/Enums/ShadowCasting.h"
#include "DingoEngine/Graphics/Enums/RootMotion.h"
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
#include <vector>

namespace Dingo
{

	class Material; // referenced by MeshRendererComponent (pointer only)
	class ParticleEffect; // referenced by ParticleEmitterComponent (pointer only)
	class Model;    // referenced by SkinnedMeshRendererComponent (pointer only)
	struct Transform3DComponent; // the light components' ToLight, defined after it

	// Identity ----------------------------------------------------------------

	/// The entity's stable identity; every entity has one from creation and it cannot be removed.
	struct IDComponent
	{
		UUID ID; ///< Unique within the scene; Scene::GetEntityByUUID finds the entity by it.

		/// Leaves ID as a default-constructed UUID.
		IDComponent() = default;
		/// Copies the ID.
		IDComponent(const IDComponent&) = default;
		/// Holds the given ID.
		IDComponent(UUID id) : ID(id) {}
	};

	/// The entity's name; every entity has one from creation and it cannot be removed.
	struct TagComponent
	{
		/// UTF-8 name; set it through Entity::SetName, which turns "" into "Entity".
		std::string Tag;

		/// Starts with an empty Tag.
		TagComponent() = default;
		/// Copies the Tag.
		TagComponent(const TagComponent&) = default;
		/// Starts with the given Tag, as is.
		TagComponent(const std::string& tag) : Tag(tag) {}
	};

	// Spatial -----------------------------------------------------------------

	/// 2D-oriented transform. Position is the center of the entity (matching the
	/// Renderer2D quad convention); Size is the full extent in world units; Rotation
	/// is in degrees about the +Z axis. Position and Rotation are relative to the
	/// entity's parent when it has one (Entity::SetParent); Size never is.
	/// Every entity gets one on creation.
	struct TransformComponent
	{
		/// Center in world units (local to the parent); z is the draw order.
		glm::vec3 Position{ 0.0f };
		float Rotation = 0.0f; ///< Degrees about +Z, counter-clockwise (local to the parent).
		glm::vec2 Size{ 1.0f }; ///< Full width and height in world units; never inherited.

		/// Position (0, 0, 0), no rotation, Size (1, 1).
		TransformComponent() = default;
		/// Copies all three fields.
		TransformComponent(const TransformComponent&) = default;
		/// Starts at `position` with no rotation and the given Size.
		TransformComponent(const glm::vec3& position, const glm::vec2& size = glm::vec2(1.0f))
			: Position(position), Size(size) {}

		/// Local matrix: translate, then rotate about +Z, then scale by Size; ignores the parent.
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

	/// A quad drawn by Renderer2D at the entity's TransformComponent (Size is the quad's size).
	struct SpriteRendererComponent
	{
		/// RGBA tint, multiplied with the texture; white leaves it unchanged.
		glm::vec4 Color{ 1.0f };
		/// Optional; null draws a solid-colour quad. Not owned: the asset system or game keeps it
		/// alive.
		Dingo::Texture* Texture = nullptr;

		/// A white quad without a texture.
		SpriteRendererComponent() = default;
		/// Copies the colour and the (non-owning) texture pointer.
		SpriteRendererComponent(const SpriteRendererComponent&) = default;
		/// A solid quad of the given RGBA colour.
		SpriteRendererComponent(const glm::vec4& color) : Color(color) {}
	};

	/// A circle drawn by Renderer2D inside the entity's TransformComponent quad (Size sets the
	/// diameter).
	struct CircleRendererComponent
	{
		glm::vec4 Color{ 1.0f }; ///< RGBA colour.
		float Thickness = 1.0f; ///< 1 is a filled disc; smaller values draw a ring.
		float Fade = 0.005f; ///< Edge softness; larger values blur the outline.

		/// A filled white circle with a slightly soft edge.
		CircleRendererComponent() = default;
		/// Copies all fields.
		CircleRendererComponent(const CircleRendererComponent&) = default;
	};

	/// Drawn at the entity's world position and turned by its world rotation (TransformComponent),
	/// in z order with sprites and circles; at equal z, above them.
	struct TextComponent
	{
		/// UTF-8 string; see Renderer2D::DrawText for the characters the atlas covers.
		std::string Text;
		/// The font to draw with; not owned. Null draws nothing.
		Dingo::Font* Font = nullptr;
		glm::vec4 Color{ 1.0f }; ///< RGBA colour.
		float Size = 1.0f; ///< Text size in world units, as Renderer2D::DrawText's size.
		bool Centered = false; ///< When true, the text is horizontally centered on Position.

		/// Empty white text at size 1, with no font.
		TextComponent() = default;
		/// Copies all fields; the font pointer is shared, not owned.
		TextComponent(const TextComponent&) = default;
	};

	// Camera ------------------------------------------------------------------

	/// The scene's camera, read by the SceneRenderer. The projection is computed
	/// from these fields (the viewport aspect is supplied by the renderer); the VIEW
	/// comes from the camera entity's transform - an Orthographic camera uses the
	/// entity's 2D TransformComponent, a Perspective camera its Transform3DComponent.
	/// Mark exactly one camera Primary per scene.
	struct CameraComponent
	{
		/// Which projection and which transform component the camera uses.
		enum class ProjectionType
		{
			Orthographic, ///< 2D view from TransformComponent, projected with OrthographicSize.
			Perspective   ///< 3D view from Transform3DComponent, projected with FOV.
		};

		/// Selects the 2D or the 3D render pass.
		ProjectionType Type = ProjectionType::Orthographic;

		/// Orthographic: full visible height in world units (width follows aspect).
		float OrthographicSize = 10.0f;
		/// Orthographic near plane distance in world units (negative reaches behind).
		float OrthoNear = -1.0f;
		float OrthoFar = 1.0f; ///< Orthographic far plane distance in world units.

		/// Perspective: vertical field of view in degrees.
		float FOV = 45.0f;
		float PerspNear = 0.1f; ///< Perspective near plane in world units; must be greater than 0.
		/// Perspective far plane in world units; beyond it nothing draws.
		float PerspFar = 1000.0f;

		/// Marks the camera the scene renders through; only one per scene should.
		bool Primary = true;

		/// An orthographic, primary camera showing 10 world units of height.
		CameraComponent() = default;
		/// Copies all fields.
		CameraComponent(const CameraComponent&) = default;

		/// Projection for the given viewport aspect (width / height). Uses the same glm
		/// calls as the rest of the engine, so the depth convention matches.
		glm::mat4 GetProjection(float aspect) const
		{
			if (Type == ProjectionType::Perspective)
				return glm::perspective(glm::radians(FOV), aspect, PerspNear, PerspFar);

			const float halfHeight = OrthographicSize * 0.5f;
			const float halfWidth = halfHeight * aspect;
			return glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, OrthoNear, OrthoFar);
		}
	};

	/// The post chain (tone mapping) for the 3D pass of the camera on this entity. SceneRenderer
	/// reads it from the primary perspective camera; on any other entity it does nothing.
	/// Settings.Enabled is false by default, which renders exactly as without it.
	struct PostProcessComponent
	{
		PostProcessSettings Settings; ///< The chain's settings, handed to the renderer as they are.

		/// Default settings, with the chain disabled.
		PostProcessComponent() = default;
		/// Copies the settings.
		PostProcessComponent(const PostProcessComponent&) = default;
		/// Starts with the given settings.
		explicit PostProcessComponent(const PostProcessSettings& settings) : Settings(settings) {}
	};

	// Lighting ----------------------------------------------------------------
	//
	// The SceneRenderer submits these to Renderer3D every frame (Scene::SubmitLights); see
	// Graphics/Light.h for falloff and the per-scene light budget. A scene without a single light
	// component is lit by a default DirectionalLightComponent, so a 3D scene never renders black
	// by accident. Any light component, even a disabled one, turns that default off. Point and
	// spot lights take their position, and a spot its aim, from the entity's world transform
	// (its Transform3DComponent through any parents), and are ignored without one.

	/// A sun-like light: parallel rays with no falloff and no position. Its Direction is a
	/// world-space vector; the entity's transform does not turn it.
	struct DirectionalLightComponent
	{
		glm::vec3 Direction{ -0.4f, -1.0f, -0.35f }; ///< World-space direction the light travels.
		glm::vec3 Color{ 1.0f }; ///< RGB colour, multiplied by Intensity.
		float Intensity = 1.0f; ///< Brightness multiplier.
		/// Legacy: white ambient this light adds, with the light scaled by (1 - Ambient).
		[[deprecated("use an AmbientLightComponent; DirectionalLightComponent::Ambient is removed in 2.0")]]
		float Ambient = 0.0f;
		/// See DirectionalLight: the first casting one gets cascaded shadows.
		bool CastShadows = false;
		float ShadowStrength = 1.0f; ///< How dark its shadows are, 0 (none) to 1 (full).
		/// Caster groups this light's shadows take; a mesh casts only if its ShadowGroups share a
		/// bit.
		uint32_t ShadowCasterGroups = 0xFFFFFFFFu;

		// The special members touch Ambient; only a game's own use of it should warn.
		DE_SUPPRESS_DEPRECATED_BEGIN
		/// A white sun of intensity 1 pointing down and slightly across, casting no shadows.
		DirectionalLightComponent() = default;
		/// Copies all fields, including the deprecated Ambient, without a warning.
		DirectionalLightComponent(const DirectionalLightComponent&) = default;
		/// Copies all fields, including the deprecated Ambient, without a warning.
		DirectionalLightComponent& operator=(const DirectionalLightComponent&) = default;
		DE_SUPPRESS_DEPRECATED_END
	};

	/// Light that reaches every face equally. Every ambient source in a scene adds up.
	struct AmbientLightComponent
	{
		glm::vec3 Color{ 1.0f }; ///< RGB colour, multiplied by Intensity.
		float Intensity = 0.1f; ///< Brightness multiplier; 0.1 is a faint fill.

		/// White ambient light at intensity 0.1.
		AmbientLightComponent() = default;
		/// Copies all fields.
		AmbientLightComponent(const AmbientLightComponent&) = default;
		/// Ambient light of the given RGB colour and intensity.
		AmbientLightComponent(const glm::vec3& color, float intensity)
			: Color(color), Intensity(intensity) {}
	};

	/// Light from the entity's position in every direction, reaching zero at Range.
	struct PointLightComponent
	{
		glm::vec3 Color{ 1.0f }; ///< RGB colour, multiplied by Intensity.
		float Intensity = 1.0f; ///< Brightness at the light; it falls off to zero at Range.
		float Range = 10.0f; ///< Distance in world units at which the light reaches zero.
		bool Enabled = true; ///< False switches the light off while keeping its settings.
		/// See PointLight: casting lights share the scene's shadow slots by rank.
		bool CastShadows = false;
		float ShadowStrength = 1.0f; ///< How dark its shadows are, 0 (none) to 1 (full).
		uint32_t ShadowCasterGroups = 0xFFFFFFFFu; ///< See DirectionalLight.

		/// A white light of intensity 1 and range 10.
		PointLightComponent() = default;
		/// Copies all fields.
		PointLightComponent(const PointLightComponent&) = default;
		/// A light of the given RGB colour, intensity and range in world units.
		PointLightComponent(const glm::vec3& color, float intensity, float range)
			: Color(color), Intensity(intensity), Range(range) {}

		/// The light Scene::SubmitLights draws for this component, placed at the transform's
		/// position. Enabled is not consulted. The transform is in world space: under a parent,
		/// pass the entity's GetWorldPosition/GetWorldRotation rather than its local component.
		PointLight ToLight(const Transform3DComponent& transform) const;
	};

	/// A cone of light from the entity's position, reaching zero at Range. Direction is in the
	/// entity's local space, so rotating the entity aims the cone; by default it points along the
	/// entity's forward axis.
	struct SpotLightComponent
	{
		glm::vec3 Color{ 1.0f }; ///< RGB colour, multiplied by Intensity.
		float Intensity = 1.0f; ///< Brightness at the light, inside the inner cone.
		float Range = 10.0f; ///< Distance in world units at which the light reaches zero.
		float InnerConeAngle = 20.0f; ///< Degrees from the axis at full strength.
		float OuterConeAngle = 30.0f; ///< Degrees from the axis where it reaches zero.
		/// Local-space aim, turned by the entity's world rotation.
		glm::vec3 Direction{ 0.0f, 0.0f, -1.0f };
		bool Enabled = true; ///< False switches the light off while keeping its settings.
		/// See SpotLight.
		bool CastShadows = false;
		float ShadowStrength = 1.0f; ///< How dark its shadows are, 0 (none) to 1 (full).
		uint32_t ShadowCasterGroups = 0xFFFFFFFFu; ///< See DirectionalLight.

		/// A white 20/30 degree cone of intensity 1 and range 10, aimed along the entity's -Z.
		SpotLightComponent() = default;
		/// Copies all fields.
		SpotLightComponent(const SpotLightComponent&) = default;
		/// A cone of the given RGB colour, intensity and range, with the default cone angles.
		SpotLightComponent(const glm::vec3& color, float intensity, float range)
			: Color(color), Intensity(intensity), Range(range) {}

		/// The light Scene::SubmitLights draws for this component: the transform's position, aimed
		/// along Rotation * Direction. Enabled is not consulted. The transform is in world space,
		/// as for PointLightComponent::ToLight.
		SpotLight ToLight(const Transform3DComponent& transform) const;
	};

	/// Distance fog over the scene's 3D pass: Settings go to Renderer3D::SetFog as they are (see
	/// Fog in Graphics/Light.h). One on the camera entity passed to Scene::SubmitLights (the
	/// SceneRenderer's perspective camera) fogs that camera's view; otherwise the first enabled one
	/// in entity order on an entity without a CameraComponent counts. One on another camera's
	/// entity fogs only a pass that names no camera, and only when there is no other. More than one
	/// candidate warns once. It is no light: a scene whose only light-like component is a
	/// FogComponent keeps the default light. With UseClearColor the fog takes the scene's clear
	/// colour (Scene::SetClearColor) in place of Settings.Color, so distant meshes fade into the
	/// background.
	struct FogComponent
	{
		Fog Settings; ///< Mode, distances, density and colour, as for Renderer3D::SetFog.
		/// True fades to the scene's clear colour instead of Settings.Color.
		bool UseClearColor = true;
		/// False takes this fog out of the choice without removing the component.
		bool Enabled = true;

		/// Linear fog from 10 to 50 world units, fading to the clear colour.
		FogComponent() = default;
		/// Copies all fields.
		FogComponent(const FogComponent&) = default;
		/// Fog with the given settings; useClearColor chooses the clear colour over Settings.Color.
		explicit FogComponent(const Fog& settings, bool useClearColor = true)
			: Settings(settings), UseClearColor(useClearColor) {}
		/// Fog of the given mode with Start and End in world units, fading to the clear colour.
		FogComponent(FogMode mode, float start, float end)
		{
			Settings.Mode = mode;
			Settings.Start = start;
			Settings.End = end;
		}
	};

	// Physics -----------------------------------------------------------------

	/// A 2D rigid body. The simulating body lives in the Scene's Physics2D world
	/// (a Box2D backend, kept entirely inside the engine); Scene::GetRuntimeBody2D
	/// returns an opaque handle to it while the scene's physics is running. No backend
	/// type ever appears in this public header.
	struct RigidBody2DComponent
	{
		/// Alias the backend-agnostic physics enum so existing references such as
		/// RigidBody2DComponent::BodyType::Dynamic keep working.
		using BodyType = BodyType2D;

		/// Static, Kinematic or Dynamic; read when the body is created.
		BodyType Type = BodyType::Static;
		bool FixedRotation = false; ///< Lock rotation about Z (e.g. a player character).

		/// A static body that may rotate.
		RigidBody2DComponent() = default;
		/// Copies all fields.
		RigidBody2DComponent(const RigidBody2DComponent&) = default;
		/// A body of the given type.
		RigidBody2DComponent(BodyType type) : Type(type) {}
	};

	/// A box collision shape for an entity that also has a RigidBody2DComponent.
	/// HalfExtents is expressed as a fraction of TransformComponent::Size,
	/// so the default { 0.5, 0.5 } exactly covers the entity's quad. Offset is in
	/// the same fractional units, relative to the entity center.
	struct BoxCollider2DComponent
	{
		/// Shape centre relative to the entity centre, in fractions of Size.
		glm::vec2 Offset{ 0.0f };
		glm::vec2 HalfExtents{ 0.5f }; ///< Half width and height, in fractions of Size.

		float Density = 1.0f; ///< Mass per unit area; only matters on a Dynamic body.
		float Friction = 0.5f; ///< Coulomb friction coefficient, 0 (ice) and up.
		float Restitution = 0.0f; ///< Bounciness, 0 (none) to 1 (perfectly elastic).

		/// A box covering the entity's quad.
		BoxCollider2DComponent() = default;
		/// Copies all fields.
		BoxCollider2DComponent(const BoxCollider2DComponent&) = default;
	};

	/// A circle collision shape. Radius is a fraction of TransformComponent::Size.x,
	/// so the default 0.5 inscribes the entity's quad.
	struct CircleCollider2DComponent
	{
		/// Shape centre relative to the entity centre, in fractions of Size.
		glm::vec2 Offset{ 0.0f };
		float Radius = 0.5f; ///< Radius as a fraction of Size.x.

		float Density = 1.0f; ///< Mass per unit area; only matters on a Dynamic body.
		float Friction = 0.5f; ///< Coulomb friction coefficient, 0 (ice) and up.
		float Restitution = 0.0f; ///< Bounciness, 0 (none) to 1 (perfectly elastic).

		/// A circle inscribed in the entity's quad.
		CircleCollider2DComponent() = default;
		/// Copies all fields.
		CircleCollider2DComponent(const CircleCollider2DComponent&) = default;
	};

	// 3D spatial / rendering / physics ----------------------------------------
	//
	// These are the 3D counterparts to the 2D components above. A 3D entity uses a
	// Transform3DComponent (the default TransformComponent it receives on creation
	// is 2D and simply goes unused); it is rendered through Renderer3D when it also
	// has a MeshRendererComponent, and simulated in the Scene's Physics3D world when
	// it has a RigidBody3DComponent plus a collider.

	/// 3D transform. Position is the entity center; Rotation is a quaternion; Scale
	/// is the full extent multiplier per axis. All three are relative to the entity's parent
	/// when it has one (Entity::SetParent); Entity::GetWorldTransform gives the world values.
	/// The Scene writes the simulated position/rotation back here each frame while 3D
	/// physics is running.
	struct Transform3DComponent
	{
		glm::vec3 Position{ 0.0f }; ///< Entity centre in world units (local to the parent).
		/// Identity (w, x, y, z); local to the parent.
		glm::quat Rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
		/// Per-axis multiplier on the mesh's own size (local to the parent).
		glm::vec3 Scale{ 1.0f };

		/// At the origin, unrotated, scale 1.
		Transform3DComponent() = default;
		/// Copies all fields.
		Transform3DComponent(const Transform3DComponent&) = default;
		/// At `position`, unrotated, with the given scale.
		Transform3DComponent(const glm::vec3& position, const glm::vec3& scale = glm::vec3(1.0f))
			: Position(position), Scale(scale) {}

		/// Local matrix, translate * rotate * scale; ignores the parent (see
		/// Entity::GetWorldTransform).
		glm::mat4 GetTransform() const
		{
			// The product written out: for finite values it equals the three matrices multiplied, up to the sign of a zero.
			const glm::mat3 rotation = glm::mat3_cast(Rotation);
			return glm::mat4(glm::vec4(rotation[0] * Scale.x, 0.0f), glm::vec4(rotation[1] * Scale.y, 0.0f),
				glm::vec4(rotation[2] * Scale.z, 0.0f), glm::vec4(Position, 1.0f));
		}

		/// Authoring convenience: set the rotation from XYZ Euler angles in degrees.
		void SetRotationEuler(const glm::vec3& eulerDegrees)
		{
			Rotation = glm::quat(glm::radians(eulerDegrees));
		}

		/// Engine-wide camera/listener convention: local -Z is forward, +Y is up. This is the
		/// local rotation, so under a parent it is not the world direction.
		glm::vec3 Forward() const { return Rotation * glm::vec3(0.0f, 0.0f, -1.0f); }
		/// Local +Y turned by Rotation; see Forward.
		glm::vec3 Up() const { return Rotation * glm::vec3(0.0f, 1.0f, 0.0f); }
	};

	inline PointLight PointLightComponent::ToLight(const Transform3DComponent& transform) const
	{
		return PointLight{ .Position = transform.Position, .Color = Color, .Intensity = Intensity, .Range = Range,
			.CastShadows = CastShadows, .ShadowStrength = ShadowStrength, .ShadowCasterGroups = ShadowCasterGroups };
	}

	inline SpotLight SpotLightComponent::ToLight(const Transform3DComponent& transform) const
	{
		return SpotLight{ .Position = transform.Position, .Direction = transform.Rotation * Direction, .Color = Color,
			.Intensity = Intensity, .Range = Range, .InnerConeAngle = InnerConeAngle, .OuterConeAngle = OuterConeAngle,
			.CastShadows = CastShadows, .ShadowStrength = ShadowStrength, .ShadowCasterGroups = ShadowCasterGroups };
	}

	/// A renderable mesh drawn by Renderer3D at the entity's Transform3D, tinted by
	/// Color. The mesh is not owned by the component (the game/asset system owns it),
	/// exactly like SpriteRendererComponent's Texture.
	struct MeshRendererComponent
	{
		/// The mesh to draw; not owned. Null draws nothing.
		Dingo::Mesh* Mesh = nullptr;
		/// RGBA tint written into the vertex stream; white leaves the material unchanged.
		glm::vec4 Color{ 1.0f };

		/// When false, the SceneRenderer skips this entity - cheap per-entity culling that
		/// replaces the old "set Mesh = nullptr and restore it later" juggling. Visible by default.
		bool Visible = true;

		/// Optional material (custom shader + uniforms + textures). Null draws with
		/// Renderer3D's built-in lit material. The Color above is written
		/// into the vertex stream either way. Owned by the client, not the component.
		Dingo::Material* Material = nullptr;

		/// ShadowsOnly keeps a mesh out of the picture but in the shadows; Visible = false drops
		/// both.
		ShadowCasting Shadows = ShadowCasting::On;
		/// Bit mask of caster groups; a light casts the mesh only when it shares a bit with the
		/// light's ShadowCasterGroups.
		uint32_t ShadowGroups = 1;
		/// Past this distance from a perspective camera the mesh is neither drawn nor casts; 0
		/// never. Renderer3D culls what is outside the view by itself.
		float CullDistance = 0.0f;
		/// This mesh's emissive, roughness and specular in place of its lit material's (the default
		/// material's when Material is null), so entities that differ only in these share a batch.
		/// Ignored with a custom material.
		std::optional<SurfaceOverride> Surface;

		/// For a mesh that doesn't move: Scene::RenderEntities3D bakes it into world space once,
		/// with the scene's other static meshes, into GPU buffers kept between frames
		/// (Renderer3D::CreateStaticBatch), instead of transforming it every frame. Moving it
		/// rebakes its cell of the scene and warns once; changing its mesh, colour, material,
		/// surface or shadows (or hot-reloading the mesh) rebakes it quietly. A mesh with a
		/// CullDistance is drawn as if this were false.
		bool Static = false;

		/// Overrides the lit material's emissive, roughness and specular for this mesh; returns
		/// *this.
		MeshRendererComponent& SetSurface(const SurfaceOverride& surface) { Surface = surface; return *this; }
		/// Goes back to the material's own surface values; returns *this.
		MeshRendererComponent& ClearSurface() { Surface.reset(); return *this; }

		/// No mesh, white, visible, casting shadows into group 1.
		MeshRendererComponent() = default;
		/// Copies all fields; the mesh and material pointers are shared, not owned.
		MeshRendererComponent(const MeshRendererComponent&) = default;
		/// Draws `mesh` tinted by `color`; the mesh stays owned by the caller.
		MeshRendererComponent(Dingo::Mesh* mesh, const glm::vec4& color = glm::vec4(1.0f))
			: Mesh(mesh), Color(color) {}
	};

	/// Draws every submesh of a Model at the entity's world transform: skinned submeshes on the GPU
	/// (Renderer3D::SubmitSkinnedMesh) posed by the entity's AnimatorComponent, or in the
	/// skeleton's rest pose without one, the rest like a MeshRendererComponent. The Model is not
	/// owned by the component. Material works as on MeshRendererComponent and applies to every
	/// submesh; the submeshes' own diffuse textures are not used.
	struct SkinnedMeshRendererComponent
	{
		/// The model to draw; not owned. Null draws nothing.
		Dingo::Model* Model = nullptr;
		glm::vec4 Color{ 1.0f }; ///< RGBA tint, as on MeshRendererComponent.
		/// Optional material applied to every submesh; null uses the built-in lit material. Not
		/// owned.
		Dingo::Material* Material = nullptr;
		bool Visible = true; ///< False skips the entity: neither drawn nor casting shadows.
		ShadowCasting Shadows = ShadowCasting::On; ///< See MeshRendererComponent.
		uint32_t ShadowGroups = 1; ///< See MeshRendererComponent.
		/// See MeshRendererComponent; the animator still runs while culled.
		float CullDistance = 0.0f;

		/// No model, white, visible, casting shadows into group 1.
		SkinnedMeshRendererComponent() = default;
		/// Copies all fields; the model and material pointers are shared, not owned.
		SkinnedMeshRendererComponent(const SkinnedMeshRendererComponent&) = default;
		/// Draws `model` tinted by `color`; the model stays owned by the caller.
		SkinnedMeshRendererComponent(Dingo::Model* model, const glm::vec4& color = glm::vec4(1.0f))
			: Model(model), Color(color) {}
	};

	/// What an AnimatorComponent does while none of its entity's skinned meshes was drawn, into the
	/// view or a shadow, by the renderers that drew the scene last frame (the renderer's culling
	/// decides). Time, fades, events and root motion always move on; only sampling and composing
	/// the pose stop.
	enum class AnimatorCullingMode
	{
		AlwaysAnimate,    ///< Poses every update, seen or not.
		PauseWhenCulled,  ///< Keeps the last pose until the entity is drawn again.
		ReduceWhenCulled, ///< Poses every 4th update.
	};

	/// Settings for the entity's Animator, which poses its SkinnedMeshRendererComponent::Model;
	/// Scene::GetAnimator returns it for playing clips from a script. Scene::OnUpdate advances it
	/// after the scripts and before physics, by deltaTime x Speed while Enabled.
	struct AnimatorComponent
	{
		/// Played, looping, when the animator is created (or its model changes) if PlayOnStart is
		/// set.
		std::string DefaultClip;
		/// Whether DefaultClip starts by itself; an empty DefaultClip plays nothing.
		bool PlayOnStart = true;
		float Speed = 1.0f; ///< Playback-rate multiplier on the scene's delta time; 1 is normal.
		/// False holds the current pose.
		bool Enabled = true;
		/// Off plays clips in place. Otherwise the clips' root travel is taken out of the pose and
		/// moves this entity: its Transform3DComponent, its character controller's velocity and
		/// rotation, or its kinematic body through MoveKinematic. A dynamic or static body isn't
		/// moved (warned once). It sets the animator's mode every frame, so set it here rather than
		/// on the Animator. A controller it stops driving (disabled, Off, not applied) has its
		/// horizontal velocity zeroed.
		RootMotionMode RootMotion = RootMotionMode::Off;
		/// False takes the travel out of the pose without moving the entity, for a script to apply
		/// Animator::GetRootMotionDelta itself.
		bool ApplyRootMotion = true;
		/// Joint sockets, bodies baked on joints and Animator::GetSkinningPalette read the last
		/// pose evaluated, so a culled animator's children hold still too. Visibility is last
		/// frame's, so an entity coming into view shows its stale pose for a frame. A scene no
		/// renderer has drawn counts as seen.
		AnimatorCullingMode CullingMode = AnimatorCullingMode::AlwaysAnimate;

		/// No default clip; plays on start at speed 1, enabled, with root motion off.
		AnimatorComponent() = default;
		/// Copies all fields.
		AnimatorComponent(const AnimatorComponent&) = default;
		/// Sets DefaultClip to the named clip, which loops once the animator is created.
		AnimatorComponent(std::string defaultClip) : DefaultClip(std::move(defaultClip)) {}
	};

	/// GPU particles from the entity's world transform (through any parent, so an emitter on a
	/// joint socket follows the joint), drawn by the SceneRenderer's 3D pass. Scene::OnUpdate steps
	/// it by the scene's capped delta, so a paused scene freezes its particles.
	/// Scene::EmitParticles and EmitParticlesAt fire bursts. The effect is the game's and must
	/// outlive the component. Needs a Transform3DComponent on the entity; without one it never draws.
	struct ParticleEmitterComponent
	{
		/// The effect to emit; not owned and must outlive the component. Null emits nothing.
		ParticleEffect* Effect = nullptr;
		/// Stopped, it spawns nothing at its rate and its particles live out their lives; started
		/// again, it emits the effect's BurstOnPlay.
		bool Playing = true;
		/// Multiplier on the effect's emission rate; 1 is as authored, 0 stops the rate.
		float RateScale = 1.0f;
		/// Off, the particles move with the entity.
		bool WorldSpace = true;

		/// No effect; playing, rate scale 1, world space.
		ParticleEmitterComponent() = default;
		/// Copies all fields; the effect pointer is shared, not owned.
		ParticleEmitterComponent(const ParticleEmitterComponent&) = default;
		/// Emits `effect`, which the caller keeps alive for as long as the component exists.
		ParticleEmitterComponent(ParticleEffect* effect) : Effect(effect) {}
	};

	/// Particles fired straight from the entity's animation events (its AnimatorComponent's clips):
	/// an instant event bursts an emitter entity, a range plays one while it is open. An emitter is
	/// any entity with a ParticleEmitterComponent, usually parented to a joint socket; a range's
	/// emitter should start with Playing off. A RangeEnd stops it, including the ones a rebind or a
	/// removed animator sends, and so does removing this component.
	struct ParticleEventComponent
	{
		/// One animation event wired to one emitter entity.
		struct Binding
		{
			std::string Event; ///< Animation event name to react to.
			/// UUID of the emitter entity (IDComponent::ID); a missing entity is skipped.
			UUID Emitter;
			uint32_t Count = 1; ///< An instant event's burst size; unused for a range.
			/// True for a RangeBegin/RangeEnd event that plays the emitter while open.
			bool Range = false;
		};
		std::vector<Binding> Bindings; ///< Checked in order for every event the animator fires.

		/// No bindings.
		ParticleEventComponent() = default;
		/// Copies the bindings.
		ParticleEventComponent(const ParticleEventComponent&) = default;

		/// Adds a binding that bursts `count` particles from `emitter` on each instant `event`;
		/// returns *this.
		ParticleEventComponent& Bind(std::string event, UUID emitter, uint32_t count)
		{
			Bindings.push_back({ std::move(event), emitter, count, false });
			return *this;
		}

		/// Adds a binding that plays `emitter` while the range event `event` is open; returns
		/// *this.
		ParticleEventComponent& BindRange(std::string event, UUID emitter)
		{
			Bindings.push_back({ std::move(event), emitter, 0, true });
			return *this;
		}
	};

	/// A 3D rigid body simulated in the Scene's Physics3D world (Jolt backend, hidden
	/// behind the Physics3D interface). Scene::GetRuntimeBody3D returns an opaque handle
	/// to it while the scene's physics is running. Unlike the 2D collider components, the 3D
	/// collider shape is baked into the body when it is created, so a 3D rigid-body
	/// entity should have exactly one Box/Sphere/Capsule/MeshCollider3DComponent alongside this;
	/// without one the body falls back to a box matching the entity's world scale.
	struct RigidBody3DComponent
	{
		/// Alias the backend-agnostic enum so RigidBody3DComponent::BodyType::Dynamic works.
		using BodyType = BodyType3D;

		/// Static, Dynamic or Kinematic; read when the body is created.
		BodyType Type = BodyType::Static;

		/// See RigidBodyParams3D::ContinuousCollision: turn on for fast bodies that must not
		/// tunnel through MeshCollider3DComponent geometry.
		bool ContinuousCollision = false;
		/// See RigidBodyParams3D: a trigger volume (Scene::GetSensorOverlaps lists what is inside),
		/// and the query layers QueryFilter3D tests.
		bool IsSensor = false;
		/// Bit mask of query layers the body is in; not a collision filter.
		std::uint32_t QueryLayers = 1u;

		/// A static body.
		RigidBody3DComponent() = default;
		/// Copies all fields.
		RigidBody3DComponent(const RigidBody3DComponent&) = default;
		/// A body of the given type.
		RigidBody3DComponent(BodyType type) : Type(type) {}
	};

	/// A box collider for an entity with a RigidBody3DComponent. HalfExtents is a
	/// fraction of the entity's world scale, so the default { 0.5, 0.5, 0.5 }
	/// exactly covers the entity's box. (Physics3D centers the shape on the body, so
	/// there is no per-collider offset - model offset with the Transform instead.)
	struct BoxCollider3DComponent
	{
		/// Half size per axis, as a fraction of the entity's world scale.
		glm::vec3 HalfExtents{ 0.5f };

		float Friction = 0.5f; ///< Coulomb friction coefficient, 0 (ice) and up.
		float Restitution = 0.0f; ///< Bounciness, 0 (none) to 1 (perfectly elastic).

		/// A box exactly covering the entity's unit box.
		BoxCollider3DComponent() = default;
		/// Copies all fields.
		BoxCollider3DComponent(const BoxCollider3DComponent&) = default;
	};

	/// A sphere collider. Radius is a fraction of the entity's world scale x, so
	/// the default 0.5 inscribes a unit box.
	struct SphereCollider3DComponent
	{
		float Radius = 0.5f; ///< Radius as a fraction of the entity's world scale x.

		float Friction = 0.5f; ///< Coulomb friction coefficient, 0 (ice) and up.
		float Restitution = 0.0f; ///< Bounciness, 0 (none) to 1 (perfectly elastic).

		/// A sphere inscribed in the entity's unit box.
		SphereCollider3DComponent() = default;
		/// Copies all fields.
		SphereCollider3DComponent(const SphereCollider3DComponent&) = default;
	};

	/// A capsule collider for an entity with a RigidBody3DComponent. The capsule stands
	/// on the +Y axis. Radius is a fraction of the entity's world scale x and
	/// HalfHeight (half the cylinder section between the caps) is a fraction of
	/// its world scale y, so on a unit-scaled entity the defaults give a
	/// 1-unit-tall capsule of radius 0.5.
	struct CapsuleCollider3DComponent
	{
		float Radius = 0.5f; ///< Radius as a fraction of the entity's world scale x.
		float HalfHeight = 0.5f; ///< Half the cylinder section, as a fraction of the world scale y.

		float Friction = 0.5f; ///< Coulomb friction coefficient, 0 (ice) and up.
		float Restitution = 0.0f; ///< Bounciness, 0 (none) to 1 (perfectly elastic).

		/// A capsule of radius 0.5 and half height 0.5, standing on +Y.
		CapsuleCollider3DComponent() = default;
		/// Copies all fields.
		CapsuleCollider3DComponent(const CapsuleCollider3DComponent&) = default;
	};

	/// A collider shaped like a Mesh, for an entity with a RigidBody3DComponent. The mesh
	/// is scaled by the entity's world scale exactly as MeshRendererComponent draws it,
	/// so the collider matches what is on screen. A null Mesh uses the entity's
	/// MeshRendererComponent::Mesh. Not owned, and only read when the body is built.
	///
	/// Convex = false collides against the triangles themselves, for level geometry:
	/// Static or Kinematic bodies only. Convex = true uses the convex hull of the vertices
	/// and works for any body type; a Dynamic body always gets the hull.
	struct MeshCollider3DComponent
	{
		/// The collision geometry; not owned. Null falls back to MeshRendererComponent::Mesh.
		Dingo::Mesh* Mesh = nullptr;
		/// True uses the convex hull (any body type); false the triangles (Static/Kinematic).
		bool Convex = false;

		float Friction = 0.5f; ///< Coulomb friction coefficient, 0 (ice) and up.
		float Restitution = 0.0f; ///< Bounciness, 0 (none) to 1 (perfectly elastic).

		/// No mesh (uses the entity's MeshRendererComponent::Mesh), triangle collision.
		MeshCollider3DComponent() = default;
		/// Copies all fields; the mesh pointer is shared, not owned.
		MeshCollider3DComponent(const MeshCollider3DComponent&) = default;
		/// Collides against `mesh`, which the caller keeps alive; `convex` picks the hull.
		MeshCollider3DComponent(Dingo::Mesh* mesh, bool convex = false)
			: Mesh(mesh), Convex(convex) {}
	};

	/// A joint between this entity's 3D body and another entity's (or the world), built when the Scene's
	/// physics starts, once every body exists. Needs a RigidBody3DComponent here; Other must have one
	/// too, or be null to pin this body to the world. An entity without a body, or an Other that names
	/// no entity or one without a body, logs once and builds nothing. The constraint lives with this
	/// entity's body: destroying either entity, or SetActive(false) on either, removes it, and
	/// reactivating rebuilds it. See ConstraintParams3D for what each field means; the Scene fills in
	/// the bodies. AnchorA is scaled by this entity's world scale like a collider, the axes are not.
	/// With Other null, AnchorB, AxisB and NormalAxisB are world values, so the default pins the body's
	/// AnchorA to the world origin. The first body of a world pin should be the moving one.
	struct Constraint3DComponent
	{
		ConstraintType3D Type = ConstraintType3D::Fixed; ///< Which joint to build.
		/// IDComponent::ID of the entity to join to; null (the default) pins to the world.
		UUID Other;

		glm::vec3 AnchorA{ 0.0f };         ///< Joint point in this entity's local space, scaled by its world scale.
		glm::vec3 AnchorB{ 0.0f };         ///< Joint point in Other's local space, or a world point when Other is null.
		glm::vec3 AxisA{ 0.0f, 1.0f, 0.0f };      ///< Hinge, twist or slider axis in this entity's local space.
		glm::vec3 AxisB{ 0.0f, 1.0f, 0.0f };      ///< The same axis in Other's local space, or in world space.
		glm::vec3 NormalAxisA{ 1.0f, 0.0f, 0.0f }; ///< A direction perpendicular to AxisA.
		glm::vec3 NormalAxisB{ 1.0f, 0.0f, 0.0f }; ///< A direction perpendicular to AxisB.

		float MinLimit = 0.0f;   ///< Hinge radians or Slider/Distance metres; see ConstraintParams3D::MinLimit.
		float MaxLimit = 0.0f;   ///< Upper limit; see ConstraintParams3D::MinLimit.
		float SwingLimitY = 0.0f; ///< SwingTwist half cone angle about the plane axis, radians.
		float SwingLimitZ = 0.0f; ///< SwingTwist half cone angle about the other swing axis, radians.
		float TwistMin = 0.0f;   ///< SwingTwist lowest twist, radians.
		float TwistMax = 0.0f;   ///< SwingTwist highest twist, radians.

		/// True lets the two bodies collide; false (the default) ignores their contacts.
		bool CollideConnected = false;

		/// A Fixed constraint to the world origin.
		Constraint3DComponent() = default;
		/// Copies all fields.
		Constraint3DComponent(const Constraint3DComponent&) = default;
		/// A constraint of the given type to `other` (null for the world).
		Constraint3DComponent(ConstraintType3D type, UUID other = UUID()) : Type(type), Other(other) {}
	};

	/// A kinematic character controller for player/enemy movement, wrapping Jolt's
	/// CharacterVirtual behind the Scene's Physics3D. The Scene creates the controller
	/// at Scene::OnStart from the params below (positioned at the entity's Transform3D),
	/// steps it each physics update, and writes its position back onto the
	/// Transform3DComponent. Scripts steer it via Scene::GetCharacterController(entity)
	/// (set velocity, read IsGrounded, etc.). An entity should have EITHER this OR a
	/// RigidBody3DComponent, not both.
	struct CharacterController3DComponent
	{
		float Radius = 0.3f;         ///< Capsule radius in world units.
		float Height = 1.8f;         ///< Full standing height in world units.
		float StepHeight = 0.3f;     ///< Max stair step-up height in world units.
		float MaxSlopeAngle = 45.0f; ///< Steepest walkable slope, degrees.
		/// Blocks and is blocked by the other controllers that set it.
		bool CollideWithCharacters = true;

		/// A 1.8 m tall, 0.3 m radius controller that steps 0.3 m and climbs 45 degree slopes.
		CharacterController3DComponent() = default;
		/// Copies all fields.
		CharacterController3DComponent(const CharacterController3DComponent&) = default;
	};

	// Audio ---------------------------------------------------------------------

	/// A sound emitter attached to an entity. Clip is a shareable decoded-audio template loaded via
	/// Application::Get().GetAudioEngine().LoadClip() - the component does not own decoding, only a
	/// reference, exactly like MeshRendererComponent's Mesh. When Spatialized is true the Scene
	/// keeps the live sound's position in sync with the entity's transform every frame (its world
	/// position if it has a Transform3DComponent, else the 2D TransformComponent at z = 0).
	struct AudioSourceComponent
	{
		/// The clip to play; shared with the audio engine. Null makes PlayAudioSource do nothing.
		std::shared_ptr<AudioClip> Clip;

		float Volume = 1.0f; ///< Linear gain; 1 is unchanged.
		float Pitch = 1.0f; ///< Playback-rate multiplier; 1 is normal.
		bool Looping = false; ///< True repeats the clip until stopped.
		/// True positions the sound in the world from the entity's transform; false plays it flat.
		bool Spatialized = true;
		/// nullopt = use the engine's current default (AudioEngine::GetDefaultAttenuation).
		/// Ignored when Spatialized is false.
		std::optional<SoundAttenuation> Attenuation;
		bool PlayOnStart = false; ///< True starts the sound when the scene starts (Scene::OnStart).
		/// The bus the sound mixes into; a stale bus plays on k_MasterBus (warned once).
		AudioBusId Bus = k_MasterBus;

		/// No clip; spatialized, not looping, volume and pitch 1, on the master bus.
		AudioSourceComponent() = default;
		/// Copies all fields; the clip is shared.
		AudioSourceComponent(const AudioSourceComponent&) = default;
	};

	/// Marks an entity as the scene's audio listener (ears for spatialized sound).
	/// Mirrors CameraComponent::Primary: the Scene uses the first Primary listener
	/// it finds, or the first listener at all if none is marked Primary. Position
	/// comes from the entity's transform; orientation only if it has a
	/// Transform3DComponent (a 2D listener stays at the engine's default
	/// orientation). If a scene has no listener entity, the AudioEngine's listener
	/// is left untouched.
	struct AudioListenerComponent
	{
		bool Primary = true; ///< Preferred over non-Primary listeners; the first Primary one wins.

		/// A primary listener.
		AudioListenerComponent() = default;
		/// Copies the Primary flag.
		AudioListenerComponent(const AudioListenerComponent&) = default;
	};

}
