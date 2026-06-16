#pragma once

#include "DingoEngine/Core/UUID.h"
#include "DingoEngine/Graphics/Texture.h"
#include "DingoEngine/Graphics/Font.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

}
