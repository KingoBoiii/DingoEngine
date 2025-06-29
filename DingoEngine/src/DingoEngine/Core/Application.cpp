#include "depch.h"
#include "DingoEngine/Core/Application.h"

#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Graphics/CommandList.h"
#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Graphics/Framebuffer.h"
#include "DingoEngine/Graphics/Pipeline.h"
#include "DingoEngine/Graphics/Shader.h"
#include "DingoEngine/Graphics/Buffer.h"

#include <glm/glm.hpp>

#define RENDER_STATIC_TRIANGLE 0
#define RENDER_VERTEX_BUFFER_TRIANGLE 0
#define RENDER_QUAD 1

#if RENDER_STATIC_TRIANGLE
struct StaticTriangleRenderPipeline
{
	DingoEngine::Shader* Shader = nullptr;
	DingoEngine::Pipeline* Pipeline = nullptr;
};
const StaticTriangleRenderPipeline SetupStaticTriangle(DingoEngine::Framebuffer* framebuffer);
void DestroyStaticTriangle(const StaticTriangleRenderPipeline& staticTriangle);
#endif // RENDER_STATIC_TRIANGLE

#if RENDER_VERTEX_BUFFER_TRIANGLE | RENDER_QUAD
struct Vertex
{
	glm::vec2 position;
	glm::vec3 color;
};
#endif 

#if RENDER_VERTEX_BUFFER_TRIANGLE
const std::vector<Vertex> vertices = {
	{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
	{{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
	{{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
};

struct VertexBufferTriangleRenderPipeline
{
	DingoEngine::Shader* Shader = nullptr;
	DingoEngine::Pipeline* Pipeline = nullptr;
	DingoEngine::VertexBuffer* VertexBuffer = nullptr;
};

const VertexBufferTriangleRenderPipeline SetupVertexBufferTriangle(DingoEngine::Framebuffer* framebuffer);
void DestroyVertexBufferTriangle(const VertexBufferTriangleRenderPipeline& staticTriangle);
#endif // RENDER_VERTEX_BUFFER_TRIANGLE

#if RENDER_QUAD
const std::vector<Vertex> vertices = {
	{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
	{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
	{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
	{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
	0, 1, 2, 2, 3, 0
};

struct QuadRenderPipeline
{
	DingoEngine::Shader* Shader = nullptr;
	DingoEngine::Pipeline* Pipeline = nullptr;
	DingoEngine::VertexBuffer* VertexBuffer = nullptr;
	DingoEngine::IndexBuffer* IndexBuffer = nullptr;
};
const QuadRenderPipeline SetupQuad(DingoEngine::Framebuffer* framebuffer);
void DestroyQuad(const QuadRenderPipeline& quadRenderPipeline);
#endif // RENDER_QUAD

namespace DingoEngine
{

	Application::Application(const ApplicationParams& params)
	{
		s_Instance = this;
	}

	Application::~Application()
	{
		s_Instance = nullptr;

		Destroy();
	}

	void Application::Initialize()
	{
		m_Window = new Window();
		m_Window->Initialize();

		OnInitialize();
	}

	void Application::Destroy()
	{
		OnDestroy();

		if (m_Window)
		{
			m_Window->Shutdown();
			delete m_Window;
			m_Window = nullptr;
		}
	}

	void Application::Run()
	{
		DingoEngine::Renderer* renderer = DingoEngine::Renderer::Create(m_Window->GetSwapChain());
		renderer->Initialize();

#if RENDER_STATIC_TRIANGLE
		const StaticTriangleRenderPipeline staticTriangleRenderPipeline = SetupStaticTriangle(m_Window->GetSwapChain()->GetFramebuffer(0));
#endif // RENDER_STATIC_TRIANGLE

#if RENDER_VERTEX_BUFFER_TRIANGLE
		const VertexBufferTriangleRenderPipeline vertexBufferTriangleRenderPipeline = SetupVertexBufferTriangle(m_Window->GetSwapChain()->GetFramebuffer(0));
#endif // RENDER_VERTEX_BUFFER_TRIANGLE

#if RENDER_QUAD
		const QuadRenderPipeline quadRenderPipeline = SetupQuad(m_Window->GetSwapChain()->GetCurrentFramebuffer());
#endif // RENDER_QUAD

		CommandListParams commandListParams = CommandListParams()
			.SetTargetSwapChain(true);

		DingoEngine::CommandList* commandList = DingoEngine::CommandList::Create(commandListParams);
		commandList->Initialize();

		while (m_Window->IsRunning())
		{
			m_Window->Update();

			renderer->BeginFrame();

#if RENDER_STATIC_TRIANGLE
			commandList->Begin();
			commandList->Clear(m_Window->GetSwapChain()->GetCurrentFramebuffer());
			commandList->Draw(m_Window->GetSwapChain()->GetCurrentFramebuffer(), staticTriangleRenderPipeline.Pipeline);
			commandList->End();
#endif // RENDER_STATIC_TRIANGLE

#if RENDER_VERTEX_BUFFER_TRIANGLE
			commandList->Begin();
			commandList->Clear();
			commandList->Draw(vertexBufferTriangleRenderPipeline.Pipeline, vertexBufferTriangleRenderPipeline.VertexBuffer);
			commandList->End();
#endif // RENDER_VERTEX_BUFFER_TRIANGLE

#if RENDER_QUAD
			commandList->Begin();
			commandList->Clear();
			commandList->DrawIndexed(quadRenderPipeline.Pipeline, quadRenderPipeline.VertexBuffer, quadRenderPipeline.IndexBuffer);
			commandList->End();
#endif // RENDER_QUAD

			renderer->EndFrame();
			renderer->Present();
			renderer->WaitAndClear();
		}

		commandList->Destroy();

#if RENDER_STATIC_TRIANGLE
		DestroyStaticTriangle(staticTriangleRenderPipeline);
#endif // RENDER_STATIC_TRIANGLE

#if RENDER_VERTEX_BUFFER_TRIANGLE
		DestroyVertexBufferTriangle(vertexBufferTriangleRenderPipeline);
#endif // RENDER_VERTEX_BUFFER_TRIANGLE

#if RENDER_QUAD
		DestroyQuad(quadRenderPipeline);
#endif // RENDER_QUAD

		renderer->Destroy();
		delete renderer;
	}

}

#if RENDER_STATIC_TRIANGLE
const StaticTriangleRenderPipeline SetupStaticTriangle(DingoEngine::Framebuffer* framebuffer)
{
	DingoEngine::ShaderParams shaderParams = DingoEngine::ShaderParams()
		.SetName("StaticTriangle")
		.AddShaderType(DingoEngine::ShaderType::Vertex, "assets/shaders/spv/static_triangle.vert.spv")
		.AddShaderType(DingoEngine::ShaderType::Fragment, "assets/shaders/spv/static_triangle.frag.spv");

	DingoEngine::Shader* shader = DingoEngine::Shader::Create(shaderParams);
	shader->Initialize();

	DingoEngine::PipelineParams pipelineParams = DingoEngine::PipelineParams()
		.SetShader(shader)
		.SetFramebuffer(framebuffer)
		.SetFillMode(DingoEngine::FillMode::Solid)
		.SetCullMode(DingoEngine::CullMode::BackAndFront);

	DingoEngine::Pipeline* pipeline = DingoEngine::Pipeline::Create(pipelineParams);
	pipeline->Initialize();

	return StaticTriangleRenderPipeline{
		.Shader = shader,
		.Pipeline = pipeline
	};
}

void DestroyStaticTriangle(const StaticTriangleRenderPipeline& staticTriangleRenderPipeline)
{
	staticTriangleRenderPipeline.Pipeline->Destroy();
	staticTriangleRenderPipeline.Shader->Destroy();
}
#endif // RENDER_STATIC_TRIANGLE

#if RENDER_VERTEX_BUFFER_TRIANGLE
const VertexBufferTriangleRenderPipeline SetupVertexBufferTriangle(DingoEngine::Framebuffer* framebuffer)
{
	DingoEngine::ShaderParams shaderParams = DingoEngine::ShaderParams()
		.SetName("GraphicsBufferTriangle_VBO")
		.AddShaderType(DingoEngine::ShaderType::Vertex, "assets/shaders/spv/graphics_buffer.vert.spv")
		.AddShaderType(DingoEngine::ShaderType::Fragment, "assets/shaders/spv/graphics_buffer.frag.spv");

	DingoEngine::Shader* shader = DingoEngine::Shader::Create(shaderParams);
	shader->Initialize();

	DingoEngine::VertexLayout vertexLayout = DingoEngine::VertexLayout()
		.SetStride(sizeof(Vertex))
		.AddAttribute("inPosition", nvrhi::Format::RG32_FLOAT, 0)
		.AddAttribute("inColor", nvrhi::Format::RGB32_FLOAT, sizeof(glm::vec2));

	DingoEngine::PipelineParams pipelineParams = DingoEngine::PipelineParams()
		.SetShader(shader)
		.SetVertexLayout(vertexLayout)
		.SetFramebuffer(framebuffer)
		.SetFillMode(DingoEngine::FillMode::Solid)
		.SetCullMode(DingoEngine::CullMode::BackAndFront);

	DingoEngine::Pipeline* pipeline = DingoEngine::Pipeline::Create(pipelineParams);
	pipeline->Initialize();

	DingoEngine::VertexBuffer* vertexBuffer = DingoEngine::VertexBuffer::Create(vertices.data(), sizeof(Vertex) * vertices.size());
	vertexBuffer->Initialize();

	return VertexBufferTriangleRenderPipeline{
		.Shader = shader,
		.Pipeline = pipeline,
		.VertexBuffer = vertexBuffer
	};
}

void DestroyVertexBufferTriangle(const VertexBufferTriangleRenderPipeline& vertexBufferTriangleRenderPipeline)
{
	vertexBufferTriangleRenderPipeline.VertexBuffer->Destroy();
	vertexBufferTriangleRenderPipeline.Pipeline->Destroy();
	vertexBufferTriangleRenderPipeline.Shader->Destroy();
}
#endif // RENDER_VERTEX_BUFFER_TRIANGLE

#if RENDER_QUAD
const QuadRenderPipeline SetupQuad(DingoEngine::Framebuffer* framebuffer)
{
	DingoEngine::ShaderParams shaderParams = DingoEngine::ShaderParams()
		.SetName("GraphicsBufferTriangle_IBO")
		.AddShaderType(DingoEngine::ShaderType::Vertex, "assets/shaders/spv/graphics_buffer.vert.spv")
		.AddShaderType(DingoEngine::ShaderType::Fragment, "assets/shaders/spv/graphics_buffer.frag.spv");

	DingoEngine::Shader* shader = DingoEngine::Shader::Create(shaderParams);
	shader->Initialize();

	DingoEngine::VertexLayout vertexLayout = DingoEngine::VertexLayout()
		.SetStride(sizeof(Vertex))
		.AddAttribute("inPosition", nvrhi::Format::RG32_FLOAT, 0)
		.AddAttribute("inColor", nvrhi::Format::RGB32_FLOAT, sizeof(glm::vec2));

	DingoEngine::PipelineParams pipelineParams = DingoEngine::PipelineParams()
		.SetShader(shader)
		.SetVertexLayout(vertexLayout)
		.SetFramebuffer(framebuffer)
		.SetFillMode(DingoEngine::FillMode::Solid)
		.SetCullMode(DingoEngine::CullMode::BackAndFront);

	DingoEngine::Pipeline* pipeline = DingoEngine::Pipeline::Create(pipelineParams);
	pipeline->Initialize();

	DingoEngine::VertexBuffer* vertexBuffer = DingoEngine::VertexBuffer::Create(vertices.data(), sizeof(Vertex) * vertices.size());
	vertexBuffer->Initialize();

	DingoEngine::IndexBuffer* indexBuffer = DingoEngine::IndexBuffer::Create(indices.data(), indices.size());
	indexBuffer->Initialize();

	return QuadRenderPipeline{
		.Shader = shader,
		.Pipeline = pipeline,
		.VertexBuffer = vertexBuffer,
		.IndexBuffer = indexBuffer
	};
}

void DestroyQuad(const QuadRenderPipeline& quadRenderPipeline)
{
	quadRenderPipeline.IndexBuffer->Destroy();
	quadRenderPipeline.VertexBuffer->Destroy();
	quadRenderPipeline.Pipeline->Destroy();
	quadRenderPipeline.Shader->Destroy();
}
#endif


