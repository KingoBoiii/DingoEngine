#pragma once
#include "DingoEngine/Graphics/Texture.h"

#include <glm/glm.hpp>

namespace Dingo
{

	class IRenderer
	{
	public:
		virtual ~IRenderer() = default;

	public:
		virtual void Initialize() = 0;
		virtual void Shutdown() = 0;

		virtual void Begin() = 0;
		virtual void End() = 0;
		
		virtual void Resize(uint32_t width, uint32_t height) = 0;
		virtual void Clear(const glm::vec4& clearColor) = 0;

		virtual Texture* GetOutput() const = 0;

	};

}
