#include "depch.h"
#include "NvrhiPipeline.h"
#include "NvrhiGraphicsBuffer.h"
#include "NvrhiTexture.h"

#include "DingoEngine/Graphics/GraphicsContext.h"
#include "NvrhiGraphicsContext.h"

namespace Dingo
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
		const auto device = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle();
		const auto nvrhiShader = static_cast<NvrhiShader*>(m_Params.Shader);

		CreateInputLayout(nvrhiShader);

		CreateBindingLayoutAndBindingSet();

		nvrhi::RasterState rasterState = nvrhi::RasterState()
			.setCullMode(Utils::ConvertCullModeToNVRHI(m_Params.CullMode))
			.setFillMode(Utils::ConvertFillModeToNVRHI(m_Params.FillMode));

		nvrhi::BlendState blendState = nvrhi::BlendState()
			.enableAlphaToCoverage();

		nvrhi::RenderState renderState = nvrhi::RenderState()
			.setRasterState(rasterState)
			.setBlendState(blendState);

		nvrhi::GraphicsPipelineDesc graphicsPipelineDesc = nvrhi::GraphicsPipelineDesc()
			.setPrimType(nvrhi::PrimitiveType::TriangleList)
			.setRenderState(renderState)
			.setInputLayout(m_InputLayoutHandle)
			.setVertexShader(nvrhiShader->m_ShaderHandles[ShaderType::Vertex])
			.setPixelShader(nvrhiShader->m_ShaderHandles[ShaderType::Pixel]);

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

	void NvrhiPipeline::CreateInputLayout(NvrhiShader* nvrhiShader)
	{
		const auto device = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle();

		if (m_Params.VertexLayout.Attributes.empty())
		{
			m_InputLayoutHandle = device->createInputLayout(nullptr, 0, nvrhiShader->m_ShaderHandles[ShaderType::Vertex]);
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

		m_InputLayoutHandle = device->createInputLayout(attributes.data(), static_cast<uint32_t>(attributes.size()), nvrhiShader->m_ShaderHandles[ShaderType::Vertex]);
	}

	void NvrhiPipeline::CreateBindingLayoutAndBindingSet()
	{
		if (m_Params.UniformBuffer == nullptr && m_Params.Texture == nullptr)
		{
			return;
		}

		nvrhi::BindingLayoutDesc bindingLayoutDesc = nvrhi::BindingLayoutDesc()
			.setRegisterSpace(0) // set = 0
			.setRegisterSpaceIsDescriptorSet(false)
			.setVisibility(nvrhi::ShaderType::All);

		nvrhi::BindingSetDesc bindingSetDesc = nvrhi::BindingSetDesc();

		if (m_Params.UniformBuffer)
		{
			bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(0));
			bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, static_cast<NvrhiGraphicsBuffer*>(m_Params.UniformBuffer)->m_BufferHandle));
		}

		if (m_Params.Texture)
		{
			//bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(0));
			//bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(0, static_cast<NvrhiTexture*>(m_Params.Texture)->m_SamplerHandle));

			bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0));
			bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, static_cast<NvrhiTexture*>(m_Params.Texture)->m_Handle));
		}

		m_BindingLayoutHandle = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createBindingLayout(bindingLayoutDesc);
		m_BindingSetHandle = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createBindingSet(bindingSetDesc, m_BindingLayoutHandle);
	}

}
