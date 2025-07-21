#pragma once

#include <glm/glm.hpp>

namespace Dingo
{

	class IRenderer2D
	{
	public:
		virtual ~IRenderer2D() = default;

	public:
		virtual void Initialize() = 0;
		virtual void Shutdown() = 0;

		virtual void Begin() = 0;
		virtual void End() = 0;

		virtual void Resize(uint32_t width, uint32_t height) = 0;
		virtual void Clear(const glm::vec4& clearColor) = 0;

		virtual void DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color) = 0;
	};

} // namespace Dingo
