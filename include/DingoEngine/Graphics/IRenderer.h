#pragma once
#include "DingoEngine/Graphics/Texture.h"
#include "DingoEngine/Graphics/Pipeline.h"
#include "DingoEngine/Graphics/GraphicsBuffer.h"

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

		virtual void Upload(GraphicsBuffer* buffer) = 0;
		virtual void Upload(GraphicsBuffer* buffer, const void* data, uint64_t size) = 0;

		virtual void Draw(Pipeline* pipeline, uint32_t vertexCount = 3, uint32_t instanceCount = 1) = 0;
		virtual void Draw(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, uint32_t vertexCount = 3, uint32_t instanceCount = 1) = 0;
		virtual void DrawIndexed(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer) = 0;
		virtual void DrawIndexed(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, GraphicsBuffer* uniformBuffer) = 0;

		virtual Texture* GetOutput() const = 0;

	};

}
