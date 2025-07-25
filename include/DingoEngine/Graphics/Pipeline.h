#pragma once
#include "Shader.h"
#include "Framebuffer.h"
#include "Enums.h"
#include "GraphicsBuffer.h"
#include "Texture.h"

namespace Dingo
{

	struct VertexLayoutAttribute
	{
		std::string Name;
		Format Format;
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

		VertexLayout& AddAttribute(const std::string& name, Format format, uint32_t offset)
		{
			Attributes.push_back({ name, format, offset });
			return *this;
		}
	};

	struct PipelineParams
	{
		std::string DebugName;
		Shader* Shader = nullptr;
		Framebuffer* Framebuffer = nullptr;
		FillMode FillMode = FillMode::Solid;
		CullMode CullMode = CullMode::Back;
		VertexLayout VertexLayout;
		GraphicsBuffer* UniformBuffer = nullptr;
		Texture* Texture = nullptr;

		PipelineParams& SetShader(Dingo::Shader* shader)
		{
			Shader = shader;
			return *this;
		}

		PipelineParams& SetFramebuffer(Dingo::Framebuffer* framebuffer)
		{
			Framebuffer = framebuffer;
			return *this;
		}

		PipelineParams& SetFillMode(Dingo::FillMode fillMode)
		{
			FillMode = fillMode;
			return *this;
		}

		PipelineParams& SetCullMode(Dingo::CullMode cullMode)
		{
			CullMode = cullMode;
			return *this;
		}

		PipelineParams& SetVertexLayout(const Dingo::VertexLayout& vertexLayout)
		{
			VertexLayout = vertexLayout;
			return *this;
		}

		PipelineParams& SetUniformBuffer(Dingo::GraphicsBuffer* uniformBuffer)
		{
			UniformBuffer = uniformBuffer;
			return *this;
		}

		PipelineParams& SetTexture(Dingo::Texture* texture)
		{
			Texture = texture;
			return *this;
		}
	};

	class Pipeline
	{
	public:
		static Pipeline* Create(Shader* shader, Framebuffer* framebuffer);
		static Pipeline* Create(const PipelineParams& params);

	public:
		Pipeline(const PipelineParams& params)
			: m_Params(params)
		{}
		virtual ~Pipeline() = default;

	public:
		virtual void Initialize() = 0;
		virtual void Destroy() = 0;

		const PipelineParams& GetParams() const { return m_Params; }

	protected:
		PipelineParams m_Params;
	};

}
