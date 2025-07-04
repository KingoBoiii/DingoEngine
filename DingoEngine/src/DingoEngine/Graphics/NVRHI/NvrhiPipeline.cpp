#include "depch.h"
#include "NvrhiPipeline.h"
#include "NvrhiGraphicsBuffer.h"

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

	void NvrhiPipeline::Initialize()
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

		if (m_BindingLayoutHandle)
		{
			graphicsPipelineDesc.addBindingLayout(m_BindingLayoutHandle);
		}

		m_GraphicsPipelineHandle = device->createGraphicsPipeline(graphicsPipelineDesc, m_Params.Framebuffer->m_FramebufferHandle);
	}

	void NvrhiPipeline::Destroy()
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

	void NvrhiPipeline::CreateInputLayout()
	{
		const auto device = GraphicsContext::GetDeviceHandle();

		if (m_Params.VertexLayout.Attributes.empty())
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

		m_InputLayoutHandle = device->createInputLayout(attributes.data(), static_cast<uint32_t>(attributes.size()), m_Params.Shader->m_ShaderHandles[ShaderType::Vertex]);
	}

	void NvrhiPipeline::CreateBindingLayoutAndBindingSet()
	{
		if (m_Params.UniformBuffer)
		{
			nvrhi::BindingLayoutDesc bindingLayoutDesc = nvrhi::BindingLayoutDesc()
				.setRegisterSpace(0) // set = 0
				.setRegisterSpaceIsDescriptorSet(false)
				.setVisibility(nvrhi::ShaderType::All) // Binding offset for the uniform buffer
				.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(0));

			m_BindingLayoutHandle = GraphicsContext::GetDeviceHandle()->createBindingLayout(bindingLayoutDesc);

			nvrhi::BindingSetDesc bindingSetDesc = nvrhi::BindingSetDesc()
				.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, static_cast<NvrhiGraphicsBuffer*>(m_Params.UniformBuffer)->m_BufferHandle));

			m_BindingSetHandle = GraphicsContext::GetDeviceHandle()->createBindingSet(bindingSetDesc, m_BindingLayoutHandle);
		}

		//bool success = nvrhi::utils::CreateBindingSetAndLayout(GraphicsContext::GetDeviceHandle(), nvrhi::ShaderType::All, 0, bindingSetDesc, m_BindingLayoutHandle, m_BindingSetHandle);
	}

}
