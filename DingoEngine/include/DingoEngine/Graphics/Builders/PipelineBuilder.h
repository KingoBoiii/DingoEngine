#pragma once
#include "DingoEngine/Graphics/Pipeline.h"

namespace DingoEngine
{

	class PipelineBuilder
	{
	public:
		PipelineBuilder() = default;
		~PipelineBuilder() = default;

	public:
		PipelineBuilder& SetDebugName(const std::string& debugName)
		{
			m_Params.DebugName = debugName;
			return *this;
		}

		PipelineBuilder& SetShader(Shader* shader)
		{
			m_Params.Shader = shader;
			return *this;
		}

		PipelineBuilder& SetFramebuffer(Framebuffer* framebuffer)
		{
			m_Params.Framebuffer = framebuffer;
			return *this;
		}

		PipelineBuilder& SetFillMode(FillMode fillMode)
		{
			m_Params.FillMode = fillMode;
			return *this;
		}

		PipelineBuilder& SetCullMode(CullMode cullMode)
		{
			m_Params.CullMode = cullMode;
			return *this;
		}

		PipelineBuilder& SetVertexLayout(const VertexLayout& vertexLayout)
		{
			m_Params.VertexLayout = vertexLayout;
			return *this;
		}

		PipelineBuilder& SetUniformBuffer(GraphicsBuffer* uniformBuffer)
		{
			m_Params.UniformBuffer = uniformBuffer;
			return *this;
		}

		PipelineBuilder& SetTexture(Texture* texture)
		{
			m_Params.Texture = texture;
			return *this;
		}

		Pipeline* Create() const
		{
			Pipeline* pipeline = Pipeline::Create(m_Params);
			pipeline->Initialize();
			return pipeline;
		}

	private:
		PipelineParams m_Params;
	};

}


