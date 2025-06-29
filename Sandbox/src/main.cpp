#include <iostream>
#include <DingoEngine.h>

#include <glm/vec3.hpp> // glm::vec3
#include <glm/vec4.hpp> // glm::vec4
#include <glm/mat4x4.hpp> // glm::mat4
#include <glm/ext/matrix_transform.hpp> // glm::translate, glm::rotate, glm::scale
#include <glm/ext/matrix_clip_space.hpp> // glm::perspective
#include <glm/ext/scalar_constants.hpp> // glm::pi

#define RENDER_STATIC_TRIANGLE 0
#define RENDER_VERTEX_BUFFER_TRIANGLE 0
#define RENDER_QUAD 1

glm::mat4 camera(float Translate, glm::vec2 const& Rotate)
{
	glm::mat4 Projection = glm::perspective(glm::pi<float>() * 0.25f, 4.0f / 3.0f, 0.1f, 100.f);
	glm::mat4 View = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -Translate));
	View = glm::rotate(View, Rotate.y, glm::vec3(-1.0f, 0.0f, 0.0f));
	View = glm::rotate(View, Rotate.x, glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 Model = glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));
	return Projection * View * Model;
}

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

int main()
{
	const auto& cam = camera(5.0f, glm::vec2(0.0f, 0.0f));

	DingoEngine::Log::Initialize();

	DE_TRACE("TRACE");
	DE_INFO("INFO");
	DE_WARN("WARN");
	DE_ERROR("ERROR");
	DE_FATAL("FATAL");

	DingoEngine::Window* window = new DingoEngine::Window();
	window->Initialize();

	DingoEngine::Renderer* renderer = DingoEngine::Renderer::Create(window->GetSwapChain());
	renderer->Initialize();

#if RENDER_STATIC_TRIANGLE
	const StaticTriangleRenderPipeline staticTriangleRenderPipeline = SetupStaticTriangle(window->GetSwapChain()->GetFramebuffer(0));
#endif // RENDER_STATIC_TRIANGLE

#if RENDER_VERTEX_BUFFER_TRIANGLE
	const VertexBufferTriangleRenderPipeline vertexBufferTriangleRenderPipeline = SetupVertexBufferTriangle(window->GetSwapChain()->GetFramebuffer(0));
#endif // RENDER_VERTEX_BUFFER_TRIANGLE

#if RENDER_QUAD
	const QuadRenderPipeline quadRenderPipeline = SetupQuad(window->GetSwapChain()->GetCurrentFramebuffer());
#endif // RENDER_QUAD

	DingoEngine::CommandList* commandList = DingoEngine::CommandList::Create();
	commandList->Initialize();

	while (window->IsRunning())
	{
		window->Update();

		renderer->BeginFrame();

#if RENDER_STATIC_TRIANGLE
		commandList->Begin();
		commandList->Clear(window->GetSwapChain()->GetCurrentFramebuffer());
		commandList->Draw(window->GetSwapChain()->GetCurrentFramebuffer(), staticTriangleRenderPipeline.Pipeline);
		commandList->End();
#endif // RENDER_STATIC_TRIANGLE

#if RENDER_VERTEX_BUFFER_TRIANGLE
		commandList->Begin();
		commandList->Clear(window->GetSwapChain()->GetCurrentFramebuffer());
		commandList->Draw(window->GetSwapChain()->GetCurrentFramebuffer(), vertexBufferTriangleRenderPipeline.Pipeline, vertexBufferTriangleRenderPipeline.VertexBuffer);
		commandList->End();
#endif // RENDER_VERTEX_BUFFER_TRIANGLE

#if RENDER_QUAD
		commandList->Begin();
		commandList->Clear(window->GetSwapChain()->GetCurrentFramebuffer());
		commandList->DrawIndexed(window->GetSwapChain()->GetCurrentFramebuffer(), quadRenderPipeline.Pipeline, quadRenderPipeline.VertexBuffer, quadRenderPipeline.IndexBuffer);
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

	window->Shutdown();
	delete window;

	DingoEngine::Log::Shutdown();

	std::cout << "Hello, world!" << std::endl;

	return 0;
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


