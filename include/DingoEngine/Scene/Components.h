#pragma once

#include "DingoEngine/Core/UUID.h"
#include "DingoEngine/Graphics/Texture.h"
#include "DingoEngine/Graphics/Font.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <string>

namespace Dingo
{

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

	// Physics -----------------------------------------------------------------

	// A 2D rigid body. The simulating body lives in the Scene's physics world
	// (Box2D, kept entirely inside the engine); RuntimeBody is an opaque handle to
	// it, valid only while the scene's physics is running. No Box2D type ever
	// appears in this public header.
	struct RigidBody2DComponent
	{
		enum class BodyType { Static = 0, Dynamic, Kinematic };

		BodyType Type = BodyType::Static;
		bool FixedRotation = false; // lock rotation about Z (e.g. a player character)

		// Opaque Box2D body handle (a b2BodyId packed into 64 bits); 0 when none.
		std::uint64_t RuntimeBody = 0;

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

		// Opaque Box2D shape handle (a b2ShapeId packed into 64 bits); 0 when none.
		std::uint64_t RuntimeShape = 0;

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

		std::uint64_t RuntimeShape = 0;

		CircleCollider2DComponent() = default;
		CircleCollider2DComponent(const CircleCollider2DComponent&) = default;
	};

}
