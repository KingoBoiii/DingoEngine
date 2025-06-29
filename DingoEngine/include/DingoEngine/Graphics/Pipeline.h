#pragma once
#include "Shader.h"
#include "Framebuffer.h"
#include "Enums.h"

namespace DingoEngine
{

	struct VertexLayoutAttribute
	{
		std::string Name;
		nvrhi::Format Format;
		uint32_t Offset = 0;
	};

	struct VertexLayout
	{
		uint32_t Stride = 0;
		std::vector<VertexLayoutAttribute> Attributes;

		VertexLayout& SetStride(uint32_t stride)
		{
			Stride = stride;
			return *this;
		}

		VertexLayout& AddAttribute(const std::string& name, nvrhi::Format format, uint32_t offset)
		{
			Attributes.push_back({ name, format, offset });
			return *this;
		}
	};

	struct PipelineParams
	{
		Shader* Shader = nullptr;
		Framebuffer* Framebuffer = nullptr;
		FillMode FillMode = FillMode::Solid;
		CullMode CullMode = CullMode::Back;
		VertexLayout VertexLayout;

		PipelineParams& SetShader(DingoEngine::Shader* shader)
		{
			Shader = shader;
			return *this;
		}

		PipelineParams& SetFramebuffer(DingoEngine::Framebuffer* framebuffer)
		{
			Framebuffer = framebuffer;
			return *this;
		}

		PipelineParams& SetFillMode(DingoEngine::FillMode fillMode)
		{
			FillMode = fillMode;
			return *this;
		}

		PipelineParams& SetCullMode(DingoEngine::CullMode cullMode)
		{
			CullMode = cullMode;
			return *this;
		}

		PipelineParams& SetVertexLayout(const DingoEngine::VertexLayout& vertexLayout)
		{
			VertexLayout = vertexLayout;
			return *this;
		}
	};

	class Pipeline
	{
	public:
		static Pipeline* Create(Shader* shader, Framebuffer* framebuffer);
		static Pipeline* Create(const PipelineParams& params);

	public:
		Pipeline(const PipelineParams& params);
		~Pipeline() = default;

	public:
		void Initialize();
		void Destroy();

		const PipelineParams& GetParams() const { return m_Params; }

	private:
		void CreateInputLayout();

	private:
		PipelineParams m_Params;
		nvrhi::InputLayoutHandle m_InputLayoutHandle;
		nvrhi::GraphicsPipelineHandle m_GraphicsPipelineHandle;

		friend class CommandList;
	};

}
