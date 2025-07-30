#pragma once

#include "DingoEngine/Graphics/CommandList.h"
#include "DingoEngine/Graphics/Framebuffer.h"

#include "DingoEngine/Graphics/Texture.h"
#include "DingoEngine/Graphics/Pipeline.h"
#include "DingoEngine/Graphics/GraphicsBuffer.h"
#include "DingoEngine/Graphics/RenderPass.h"

#include <glm/glm.hpp>

namespace Dingo
{

	struct RendererParams
	{
		bool TargetSwapChain = false; // If true, the renderer will target the swap chain for rendering
		std::string FramebufferName = "RendererFramebuffer"; // Name for the framebuffer if not targeting swap chain
	};

	class Renderer
	{
	public:
		static Renderer* Create(const RendererParams& params);

	public:
		Renderer(const RendererParams& params)
			: m_Params(params)
		{}
		virtual ~Renderer() = default;

	public:
		virtual void Initialize();
		virtual void Shutdown();

		virtual void BeginRenderPass(RenderPass* renderPass);
		virtual void EndRenderPass();

		virtual void Begin();
		virtual void End();

		virtual void Clear(Framebuffer* framebuffer, const glm::vec4& clearColor);

		virtual void Resize(uint32_t width, uint32_t height);
		virtual void Clear(const glm::vec4& clearColor);

		virtual void Upload(GraphicsBuffer* buffer);
		virtual void Upload(GraphicsBuffer* buffer, const void* data, uint64_t size);

		virtual void Draw(Pipeline* pipeline, uint32_t vertexCount = 3, uint32_t instanceCount = 1);
		virtual void Draw(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, uint32_t vertexCount = 3, uint32_t instanceCount = 1);
		virtual void DrawIndexed(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer);
		virtual void DrawIndexed(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, GraphicsBuffer* uniformBuffer);

		virtual Framebuffer* GetTargetFramebuffer() const { return m_TargetFramebuffer; }
		virtual Texture* GetOutput() const;

		const RendererParams& GetParams() const { return m_Params; }
		Texture* GetWhiteTexture() const { return m_StaticResources.WhiteTexture; }

	private:
		RendererParams m_Params;
		CommandList* m_CommandList = nullptr; // Command list for recording commands
		Framebuffer* m_TargetFramebuffer = nullptr; // The framebuffer that the renderer will render to

		struct StaticResources
		{
			Texture* WhiteTexture = nullptr; // A white texture for default rendering
		} m_StaticResources;
	};

}
