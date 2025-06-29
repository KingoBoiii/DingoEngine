#include "depch.h"
#include "DingoEngine/Graphics/Pipeline.h"
#include "DingoEngine/Graphics/GraphicsContext.h"

namespace DingoEngine
{

	namespace Utils
	{

		static nvrhi::RasterFillMode ConvertFillModeToNVRHI(FillMode fillMode)
		{
			switch (fillMode)
			{
				case FillMode::Solid: return nvrhi::RasterFillMode::Solid;
				case FillMode::Wireframe: return nvrhi::RasterFillMode::Wireframe;
				default: return nvrhi::RasterFillMode::Solid; // Default to solid if unknown
			}
		}

		static nvrhi::RasterCullMode ConvertCullModeToNVRHI(CullMode cullMode)
		{
			switch (cullMode)
			{
				case CullMode::None: return nvrhi::RasterCullMode::None;
				case CullMode::Front: return nvrhi::RasterCullMode::Front;
				case CullMode::Back: return nvrhi::RasterCullMode::Back;
				default: return nvrhi::RasterCullMode::None; // Default to back culling if unknown
			}
		}

	}

	Pipeline* Pipeline::Create(Shader* shader, Framebuffer* framebuffer)
	{
		PipelineParams params = PipelineParams()
			.SetShader(shader)
			.SetFramebuffer(framebuffer);

		return new Pipeline(params);
	}

	Pipeline* Pipeline::Create(const PipelineParams& params)
	{
		return new Pipeline(params);
	}

	Pipeline::Pipeline(const PipelineParams& params)
		: m_Params(params)
	{}

	void Pipeline::Initialize()
	{
		const auto device = GraphicsContext::GetDeviceHandle();

		CreateInputLayout();

		//nvrhi::VertexAttributeDesc attributes[] = {
		//	nvrhi::VertexAttributeDesc()
		//		.setName("POSITION")
		//		.setFormat(nvrhi::Format::RGB32_FLOAT)
		//		.setOffset(0)
		//		.setElementStride(3 * sizeof(float))
		//};

		nvrhi::RasterState rasterState = nvrhi::RasterState()
			.setCullMode(Utils::ConvertCullModeToNVRHI(m_Params.CullMode))
			.setFillMode(Utils::ConvertFillModeToNVRHI(m_Params.FillMode));

		nvrhi::RenderState renderState = nvrhi::RenderState()
			.setRasterState(rasterState);

		nvrhi::GraphicsPipelineDesc graphicsPipelineDesc = nvrhi::GraphicsPipelineDesc()
			.setPrimType(nvrhi::PrimitiveType::TriangleList)
			.setRenderState(renderState)
			.setInputLayout(m_InputLayoutHandle)
			.setVertexShader(m_Params.Shader->m_ShaderHandles[ShaderType::Vertex])
			.setPixelShader(m_Params.Shader->m_ShaderHandles[ShaderType::Pixel]);

		m_GraphicsPipelineHandle = device->createGraphicsPipeline(graphicsPipelineDesc, m_Params.Framebuffer->m_FramebufferHandle);
	}

	void Pipeline::Destroy()
	{
		if (m_InputLayoutHandle)
		{
			m_InputLayoutHandle->Release();
		}

		if (m_GraphicsPipelineHandle)
		{
			m_GraphicsPipelineHandle->Release();
		}
	}

	void Pipeline::CreateInputLayout()
	{
		const auto device = GraphicsContext::GetDeviceHandle();

		if(m_Params.VertexLayout.Attributes.empty())
		{
			m_InputLayoutHandle = device->createInputLayout(nullptr, 0, m_Params.Shader->m_ShaderHandles[ShaderType::Vertex]);
			return; // No attributes to create input layout
		}

		std::vector< nvrhi::VertexAttributeDesc> attributes;
		int32_t index = 0;
		for (const auto& attribute : m_Params.VertexLayout.Attributes)
		{
			nvrhi::VertexAttributeDesc vertexAttributeDesc = nvrhi::VertexAttributeDesc()
				.setBufferIndex(index)
				.setName(attribute.Name)
				.setFormat(attribute.Format)
				.setOffset(attribute.Offset)
				.setElementStride(m_Params.VertexLayout.Stride);

			attributes.push_back(vertexAttributeDesc);

			//index++;
		}

		m_InputLayoutHandle = device->createInputLayout(attributes.data(), attributes.size(), m_Params.Shader->m_ShaderHandles[ShaderType::Vertex]);
	}

}
