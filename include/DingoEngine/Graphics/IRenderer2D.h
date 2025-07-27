#pragma once
#include "DingoEngine/Graphics/Framebuffer.h"
#include "DingoEngine/Graphics/GraphicsBuffer.h"

#include <glm/glm.hpp>

namespace Dingo
{

	struct IRenderer2DParams
	{
		bool TargetSwapChain = false;
		Framebuffer* TargetFramebuffer = nullptr; // nullptr = use swap chain
	};

	class IRenderer2D
	{
	public:
		IRenderer2D(const IRenderer2DParams& params)
			: m_Params(params)
		{}
		virtual ~IRenderer2D() = default;

	public:
		virtual void Initialize() = 0;
		virtual void Shutdown() = 0;
		virtual void Resize(uint32_t width, uint32_t height) = 0;
		virtual void SetOutputFramebuffer(Framebuffer* framebuffer) = 0;

		virtual void Begin() = 0;
		virtual void End() = 0;

		virtual void Clear(const glm::vec4& clearColor) = 0;

		virtual void Upload(GraphicsBuffer* buffer) = 0;
		virtual void Upload(GraphicsBuffer* buffer, const void* data, uint64_t size) = 0;

		virtual void DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color) = 0;

	protected:
		IRenderer2DParams m_Params;

		struct QuadVertex
		{
			glm::vec3 Position;
			glm::vec4 Color;
		};
	};

} // namespace Dingo
