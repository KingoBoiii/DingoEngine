#include "depch.h"
#include "DingoEngine/Graphics/Pipeline.h"
#include "DingoEngine/Graphics/GraphicsContext.h"

#include <nvrhi/utils.h>

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

		CreateBindingLayoutAndBindingSet();

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

		if(m_BindingLayoutHandle)
		{
			graphicsPipelineDesc.addBindingLayout(m_BindingLayoutHandle);
		}

		m_GraphicsPipelineHandle = device->createGraphicsPipeline(graphicsPipelineDesc, m_Params.Framebuffer->m_FramebufferHandle);
	}

	void Pipeline::Destroy()
	{
		if (m_BindingSetHandle)
		{
			m_BindingSetHandle->Release();
		}

		if (m_BindingLayoutHandle)
		{
			m_BindingLayoutHandle->Release();
		}

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

	void Pipeline::CreateBindingLayoutAndBindingSet()
	{
		if(m_Params.UniformBuffer == nullptr)
		{
			return; // No uniform buffer to create binding set
		}

		//nvrhi::BindingLayoutDesc bindingLayoutDesc = nvrhi::BindingLayoutDesc()
		//	.setRegisterSpace(0) // set = 0
		//	.setRegisterSpaceIsDescriptorSet(false)
		//	.setVisibility(nvrhi::ShaderType::All)
		//	.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(0)
		//		.setSize(m_Params.UniformBuffer->m_Size));

		//m_BindingLayoutHandle = GraphicsContext::GetDeviceHandle()->createBindingLayout(bindingLayoutDesc);

		nvrhi::BindingSetDesc bindingSetDesc = nvrhi::BindingSetDesc()
			.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_Params.UniformBuffer->m_BufferHandle));

		//m_BindingSetHandle = GraphicsContext::GetDeviceHandle()->createBindingSet(bindingSetDesc, m_BindingLayoutHandle);

		bool success = nvrhi::utils::CreateBindingSetAndLayout(GraphicsContext::GetDeviceHandle(), nvrhi::ShaderType::All, 0, bindingSetDesc, m_BindingLayoutHandle, m_BindingSetHandle);
	}

}
