#pragma once

#include "DingoEngine/Graphics/CommandList.h"
#include "DingoEngine/Graphics/Framebuffer.h"

#include "DingoEngine/Graphics/Texture.h"
#include "DingoEngine/Graphics/Pipeline.h"
#include "DingoEngine/Graphics/GraphicsBuffer.h"
#include "DingoEngine/Graphics/RenderPass.h"
#include "DingoEngine/Graphics/Sampler.h"

#include <glm/glm.hpp>

namespace Dingo
{

	class Mesh;
	class PerspectiveCamera;
	struct Renderer3DState;

	struct RendererParams
	{
		Framebuffer* TargetFramebuffer = nullptr; // If not targeting swap chain, specify a framebuffer to target
	};

	class Renderer
	{
	public:
		static Renderer* Create(Framebuffer* framebuffer);
		static Renderer* Create(const RendererParams& params);

	public:
		Renderer() = delete;
		Renderer(const Renderer&) = delete;
		Renderer& operator=(const Renderer&) = delete;
		Renderer(Renderer&&) = delete;
		Renderer& operator=(Renderer&&) = delete;
		virtual ~Renderer() = default;

	public:
		virtual void Initialize();
		virtual void Shutdown();

		/**************************************************
		***		RENDER PASS	API							***
		**************************************************/

		virtual void BeginRenderPass(RenderPass* renderPass);
		virtual void EndRenderPass();

		/**************************************************
		***		DRAW CALLS								***
		**************************************************/

		virtual void DrawIndexed(GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, uint32_t indexCount = 0);
		virtual void DrawIndexed(GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, GraphicsBuffer* uniformBuffer, uint32_t indexCount = 0);

		/**************************************************
		***		GENERAL									***
		**************************************************/

		virtual void Begin();
		virtual void Close();   // Seal the command list without submitting (main thread)
		virtual void Execute(); // Submit a sealed command list to the GPU (render thread)
		virtual void End();     // Close() + Execute() — default single-threaded behaviour

		virtual void Clear(Framebuffer* framebuffer, const glm::vec4& clearColor);

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

		/**************************************************
		***		3D API									***
		**************************************************/

		// Begin a 3D scene. Must be paired with EndScene(). Clears to clearColor.
		void BeginScene(const PerspectiveCamera& camera, const glm::vec4& clearColor = { 0.1f, 0.1f, 0.15f, 1.0f });

		// Submit a mesh instance into the current 3D scene batch.
		void DrawMesh(Mesh* mesh, const glm::mat4& transform, const glm::vec4& color = glm::vec4(1.0f));

		// Flush and submit the 3D scene to the GPU.
		void EndScene();

		/**************************************************
		***		STATIC RESOURCES 						***
		**************************************************/

		static Texture* GetWhiteTexture();
		static Sampler* GetClampSampler();
		static Sampler* GetPointSampler();

	protected:
		Renderer(const RendererParams& params, bool targetSwapChain = false)
			: m_Params(params), m_TargetSwapChain(targetSwapChain)
		{}

	public:
		static void InitializeStaticResources();
		static void DestroyStaticResources();

	private:
		void InitializeMeshPipeline();
		void ShutdownMeshPipeline();

	private:
		RendererParams m_Params;
		CommandList* m_CommandList = nullptr;
		Framebuffer* m_TargetFramebuffer = nullptr;
		bool m_TargetSwapChain = false;
		Renderer3DState* m_3DState = nullptr;
	};

}
