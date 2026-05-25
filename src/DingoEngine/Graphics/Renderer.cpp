#include "depch.h"
#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Graphics/Mesh.h"
#include "DingoEngine/Graphics/Shader.h"
#include "DingoEngine/Core/PerspectiveCamera.h"
#include "DingoEngine/Core/Application.h"

#include <glm/gtc/type_ptr.hpp>

namespace Dingo
{

	namespace Shaders3D
	{

		constexpr const char* MeshShader = R"(
#type vertex
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;

layout(std140, binding = 0) uniform CameraData {
	mat4 ViewProjection;
};

layout(location = 0) out vec4 v_Color;

void main()
{
	gl_Position = ViewProjection * vec4(a_Position, 1.0);
	v_Color = a_Color;
}

#type fragment
#version 450

layout(location = 0) in vec4 v_Color;
layout(location = 0) out vec4 o_Color;

void main()
{
	o_Color = v_Color;
}
		)";

	}

	struct BatchMeshVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;
	};

	static constexpr uint32_t MaxMeshVerticesPerFrame = 65536;
	static constexpr uint32_t MaxMeshIndicesPerFrame  = 65536;

	struct Renderer3DState
	{
		struct CameraData
		{
			glm::mat4 ViewProjection;
		};
		CameraData CameraUniformData = {};
		GraphicsBuffer* CameraUBO = nullptr;

		Shader*     MeshShader     = nullptr;
		Pipeline*   MeshPipeline   = nullptr;
		RenderPass* MeshRenderPass = nullptr;

		BatchMeshVertex* VertexBufferBase = nullptr;
		BatchMeshVertex* VertexBufferPtr  = nullptr;
		GraphicsBuffer*  VertexBuffer     = nullptr;

		uint16_t*       IndexBufferBase = nullptr;
		uint16_t*       IndexBufferPtr  = nullptr;
		uint32_t        IndexCount      = 0;
		uint16_t        VertexOffset    = 0;
		GraphicsBuffer* IndexBuffer     = nullptr;

		glm::vec4 ClearColor  = { 0.1f, 0.1f, 0.15f, 1.0f };
		bool      Initialized = false;
	};

	struct StaticResources
	{
		Texture* WhiteTexture = nullptr;
		Sampler* ClampSampler = nullptr;
		Sampler* PointSampler = nullptr;
	};

	static StaticResources* s_StaticResources = nullptr;

	Renderer* Renderer::Create(Framebuffer* framebuffer)
	{
		Renderer* renderer = new Renderer(RendererParams{ .TargetFramebuffer = framebuffer });
		renderer->Initialize();
		return renderer;
	}

	Renderer* Renderer::Create(const RendererParams& params)
	{
		Renderer* renderer = new Renderer(params);
		renderer->Initialize();
		return renderer;
	}

	void Renderer::Initialize()
	{
		m_TargetFramebuffer = m_Params.TargetFramebuffer;
		if (m_TargetFramebuffer == nullptr)
		{
			m_TargetFramebuffer = Application::Get().GetSwapChain()->GetCurrentFramebuffer();
		}

		m_CommandList = CommandList::Create();
		m_3DState = new Renderer3DState();
	}

	void Renderer::Shutdown()
	{
		ShutdownMeshPipeline();

		delete m_3DState;
		m_3DState = nullptr;

		if (m_CommandList)
		{
			m_CommandList->Destroy();
			m_CommandList = nullptr;
		}
	}

	/**************************************************
	***		RENDER PASS								***
	**************************************************/

	void Renderer::BeginRenderPass(RenderPass* renderPass)
	{
		// TODO: find out if Render Pass needs to be rebaked, before setting it

		m_CommandList->SetRenderPass(renderPass);

		if (m_TargetSwapChain)
		{
			m_TargetFramebuffer = Application::Get().GetSwapChain()->GetCurrentFramebuffer();
			m_CommandList->SetFramebuffer(m_TargetFramebuffer);
		}
	}

	void Renderer::EndRenderPass()
	{
		
	}

	/**************************************************
	***		DRAW CALLS								***
	**************************************************/

	void Renderer::DrawIndexed(GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, uint32_t indexCount)
	{
		if (indexCount == 0)
		{
			indexCount = indexBuffer->GetByteSize() / sizeof(uint16_t); // Assuming 16-bit indices
		}

		m_CommandList->AddVertexBuffer(vertexBuffer, 0);
		m_CommandList->SetIndexBuffer(indexBuffer, 0);
		m_CommandList->DrawIndexed(indexCount, 1); // Assuming 16-bit indices
	}

	void Renderer::DrawIndexed(GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, GraphicsBuffer* uniformBuffer, uint32_t indexCount)
	{
		if (indexCount == 0)
		{
			indexCount = indexBuffer->GetByteSize() / sizeof(uint16_t); // Assuming 16-bit indices
		}

		m_CommandList->AddVertexBuffer(vertexBuffer, 0);
		m_CommandList->SetIndexBuffer(indexBuffer, 0);
		m_CommandList->DrawIndexed(indexCount, 1); // Assuming 16-bit indices
	}

	/**************************************************
	***		GENERAL									***
	**************************************************/

	void Renderer::Begin()
	{
		m_CommandList->Begin();
	}

	void Renderer::Close()
	{
		m_CommandList->Close();
	}

	void Renderer::Execute()
	{
		m_CommandList->Execute();
	}

	void Renderer::End()
	{
		Close();
		Execute();
	}

	void Renderer::Clear(Framebuffer* framebuffer, const glm::vec4& clearColor)
	{
		m_CommandList->Clear(framebuffer, 0, clearColor);
	}

	void Renderer::Clear(const glm::vec4& clearColor)
	{
		if (m_TargetSwapChain)
		{
			m_TargetFramebuffer = Application::Get().GetSwapChain()->GetCurrentFramebuffer();
			m_CommandList->SetFramebuffer(m_TargetFramebuffer);
		}

		m_CommandList->Clear(m_TargetFramebuffer, 0, clearColor);
	}

	void Renderer::Upload(GraphicsBuffer* buffer)
	{
		m_CommandList->UploadBuffer(buffer, buffer->GetData(), buffer->GetByteSize());
	}

	void Renderer::Upload(GraphicsBuffer* buffer, const void* data, uint64_t size)
	{
		m_CommandList->UploadBuffer(buffer, data, size);
	}

	void Renderer::Draw(Pipeline* pipeline, uint32_t vertexCount, uint32_t instanceCount)
	{
		m_CommandList->SetPipeline(pipeline);

		if (m_TargetSwapChain)
		{
			m_TargetFramebuffer = Application::Get().GetSwapChain()->GetCurrentFramebuffer();
			m_CommandList->SetFramebuffer(m_TargetFramebuffer);
		}

		m_CommandList->Draw(vertexCount, instanceCount);
	}

	void Renderer::Draw(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, uint32_t vertexCount, uint32_t instanceCount)
	{
		m_CommandList->SetPipeline(pipeline);

		if (m_TargetSwapChain)
		{
			m_TargetFramebuffer = Application::Get().GetSwapChain()->GetCurrentFramebuffer();
			m_CommandList->SetFramebuffer(m_TargetFramebuffer);
		}

		m_CommandList->AddVertexBuffer(vertexBuffer, 0);
		m_CommandList->Draw(vertexCount, instanceCount);
	}

	void Renderer::DrawIndexed(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer)
	{
		m_CommandList->SetPipeline(pipeline);

		if (m_TargetSwapChain)
		{
			m_TargetFramebuffer = Application::Get().GetSwapChain()->GetCurrentFramebuffer();
			m_CommandList->SetFramebuffer(m_TargetFramebuffer);
		}

		m_CommandList->AddVertexBuffer(vertexBuffer, 0);
		m_CommandList->SetIndexBuffer(indexBuffer, 0);
		m_CommandList->DrawIndexed(indexBuffer->GetByteSize() / sizeof(uint16_t), 1); // Assuming 16-bit indices
	}

	void Renderer::DrawIndexed(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, GraphicsBuffer* uniformBuffer)
	{
		m_CommandList->UploadBuffer(uniformBuffer, uniformBuffer->GetData(), uniformBuffer->GetByteSize());

		m_CommandList->SetPipeline(pipeline);

		if (m_TargetSwapChain)
		{
			m_TargetFramebuffer = Application::Get().GetSwapChain()->GetCurrentFramebuffer();
			m_CommandList->SetFramebuffer(m_TargetFramebuffer);
		}

		m_CommandList->AddVertexBuffer(vertexBuffer, 0);
		m_CommandList->SetIndexBuffer(indexBuffer, 0);
		m_CommandList->DrawIndexed(indexBuffer->GetByteSize() / sizeof(uint16_t), 1); // Assuming 16-bit indices
	}

	Texture* Renderer::GetOutput() const
	{
		return m_TargetFramebuffer->GetAttachment(0);
	}

	/**************************************************
	***		STATIC RESOURCES 						***
	**************************************************/

	Texture* Renderer::GetWhiteTexture()
	{
		return s_StaticResources->WhiteTexture;
	}

	Sampler* Renderer::GetClampSampler()
	{
		return s_StaticResources->ClampSampler;
	}

	Sampler* Renderer::GetPointSampler()
	{
		return s_StaticResources->PointSampler;
	}

	void Renderer::InitializeStaticResources()
	{
		if (s_StaticResources)
		{
			return;
		}

		s_StaticResources = new StaticResources();

		uint32_t whiteTextureData = 0xffffffff;
		s_StaticResources->WhiteTexture = Texture::CreateFromData(1, 1, &whiteTextureData, TextureFormat::RGBA, "White Texture");

		s_StaticResources->ClampSampler = Sampler::Create(SamplerParams());
		s_StaticResources->ClampSampler->Initialize();

		s_StaticResources->PointSampler = Sampler::Create(SamplerParams()
			.SetMinFilter(false)
			.SetMagFilter(false)
			.SetMipFilter(false));
		s_StaticResources->PointSampler->Initialize();
	}

	void Renderer::DestroyStaticResources()
	{
		if (!s_StaticResources)
		{
			return;
		}

		if (s_StaticResources->WhiteTexture)
		{
			s_StaticResources->WhiteTexture->Destroy();
			s_StaticResources->WhiteTexture = nullptr;
		}

		if (s_StaticResources->ClampSampler)
		{
			s_StaticResources->ClampSampler->Destroy();
			s_StaticResources->ClampSampler = nullptr;
		}

		if (s_StaticResources->PointSampler)
		{
			s_StaticResources->PointSampler->Destroy();
			s_StaticResources->PointSampler = nullptr;
		}

		delete s_StaticResources;
		s_StaticResources = nullptr;
	}

	/**************************************************
	***		3D PIPELINE LIFECYCLE					***
	**************************************************/

	void Renderer::InitializeMeshPipeline()
	{
		auto& s = *m_3DState;

		s.CameraUBO = GraphicsBuffer::CreateUniformBuffer(sizeof(Renderer3DState::CameraData), "Renderer3D_CameraUBO");

		s.VertexBuffer = GraphicsBuffer::CreateVertexBuffer(
			sizeof(BatchMeshVertex) * MaxMeshVerticesPerFrame, nullptr, true, "Renderer3D_VertexBuffer");
		s.IndexBuffer = GraphicsBuffer::CreateIndexBuffer(
			sizeof(uint16_t) * MaxMeshIndicesPerFrame, nullptr, true, "Renderer3D_IndexBuffer");

		s.VertexBufferBase = new BatchMeshVertex[MaxMeshVerticesPerFrame];
		s.IndexBufferBase  = new uint16_t[MaxMeshIndicesPerFrame];

		s.MeshShader = Shader::CreateFromSource("Renderer3DMeshShader", Shaders3D::MeshShader);

		VertexLayout vertexLayout = VertexLayout()
			.SetStride(sizeof(BatchMeshVertex))
			.AddAttribute("a_Position", Format::RGB32_FLOAT,  offsetof(BatchMeshVertex, Position))
			.AddAttribute("a_Color",    Format::RGBA32_FLOAT, offsetof(BatchMeshVertex, Color));

		s.MeshPipeline = Pipeline::Create(PipelineParams()
			.SetDebugName("Renderer3DMeshPipeline")
			.SetFramebuffer(GetTargetFramebuffer())
			.SetShader(s.MeshShader)
			.SetVertexLayout(vertexLayout)
			.SetCullMode(CullMode::None));

		s.MeshRenderPass = RenderPass::Create(RenderPassParams().SetPipeline(s.MeshPipeline));
		s.MeshRenderPass->Initialize();
		s.MeshRenderPass->SetUniformBuffer(0, s.CameraUBO);
		s.MeshRenderPass->Bake();

		s.Initialized = true;
	}

	void Renderer::ShutdownMeshPipeline()
	{
		if (!m_3DState || !m_3DState->Initialized)
			return;

		auto& s = *m_3DState;

		delete[] s.VertexBufferBase;
		delete[] s.IndexBufferBase;
		s.VertexBufferBase = nullptr;
		s.IndexBufferBase  = nullptr;

		if (s.VertexBuffer) { s.VertexBuffer->Destroy(); s.VertexBuffer = nullptr; }
		if (s.IndexBuffer)  { s.IndexBuffer->Destroy();  s.IndexBuffer  = nullptr; }
		if (s.CameraUBO)    { s.CameraUBO->Destroy();    s.CameraUBO    = nullptr; }

		if (s.MeshRenderPass) { s.MeshRenderPass->Destroy(); s.MeshRenderPass = nullptr; }
		if (s.MeshPipeline)   { s.MeshPipeline->Destroy();   s.MeshPipeline   = nullptr; }
		if (s.MeshShader)     { s.MeshShader->Destroy();     s.MeshShader     = nullptr; }

		s.Initialized = false;
	}

	/**************************************************
	***		3D API									***
	**************************************************/

	void Renderer::BeginScene(const PerspectiveCamera& camera, const glm::vec4& clearColor)
	{
		if (!m_3DState->Initialized)
			InitializeMeshPipeline();

		auto& s = *m_3DState;
		s.ClearColor = clearColor;
		s.CameraUniformData.ViewProjection = camera.GetViewProjectionMatrix();
		s.CameraUBO->Upload(&s.CameraUniformData, sizeof(Renderer3DState::CameraData));

		s.VertexBufferPtr = s.VertexBufferBase;
		s.IndexBufferPtr  = s.IndexBufferBase;
		s.IndexCount      = 0;
		s.VertexOffset    = 0;
	}

	void Renderer::DrawMesh(Mesh* mesh, const glm::mat4& transform, const glm::vec4& color)
	{
		DE_CORE_ASSERT(m_3DState && m_3DState->Initialized, "Call BeginScene before DrawMesh.");

		auto& s = *m_3DState;
		const auto& verts   = mesh->GetVertices();
		const auto& indices = mesh->GetIndices();

		DE_CORE_ASSERT(s.VertexOffset + verts.size()   <= MaxMeshVerticesPerFrame, "3D vertex buffer overflow");
		DE_CORE_ASSERT(s.IndexCount   + indices.size() <= MaxMeshIndicesPerFrame,  "3D index buffer overflow");

		for (const auto& v : verts)
		{
			s.VertexBufferPtr->Position = glm::vec3(transform * glm::vec4(v.Position, 1.0f));
			s.VertexBufferPtr->Color    = color;
			s.VertexBufferPtr++;
		}

		for (uint16_t idx : indices)
		{
			*s.IndexBufferPtr++ = idx + s.VertexOffset;
		}

		s.VertexOffset += static_cast<uint16_t>(verts.size());
		s.IndexCount   += static_cast<uint32_t>(indices.size());
	}

	void Renderer::EndScene()
	{
		auto& s = *m_3DState;

		// When targeting the swap chain the command list is already open for the
		// entire frame (opened by AppRenderer::BeginFrame / closed by EndFrame).
		// Only manage the scope ourselves for standalone (off-screen) renderers.
		if (!m_TargetSwapChain)
			Begin();

		Clear(s.ClearColor);

		if (s.IndexCount > 0)
		{
			uint32_t vertexDataSize = static_cast<uint32_t>(
				reinterpret_cast<uint8_t*>(s.VertexBufferPtr) - reinterpret_cast<uint8_t*>(s.VertexBufferBase));

			s.VertexBuffer->Upload(s.VertexBufferBase, vertexDataSize);
			s.IndexBuffer->Upload(s.IndexBufferBase, s.IndexCount * sizeof(uint16_t));

			Upload(s.CameraUBO);

			BeginRenderPass(s.MeshRenderPass);
			DrawIndexed(s.VertexBuffer, s.IndexBuffer, s.IndexCount);
			EndRenderPass();
		}

		if (!m_TargetSwapChain)
			End();
	}

}
