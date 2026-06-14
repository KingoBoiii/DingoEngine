#include "depch.h"
#include "NvrhiPipeline.h"
#include "NvrhiGraphicsBuffer.h"
#include "NvrhiTexture.h"
#include "NvrhiFramebuffer.h"

#include "DingoEngine/Graphics/Renderer.h"
#include "NvrhiSampler.h"

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
				case FillMode::Wireframe: return nvrhi::RasterFillMode::Line;
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

		CreateBindingSet(nvrhiShader->m_BindingLayoutHandle);

		nvrhi::RasterState rasterState = nvrhi::RasterState()
			.setCullMode(Utils::ConvertCullModeToNVRHI(m_Params.CullMode))
			.setFillMode(Utils::ConvertFillModeToNVRHI(m_Params.FillMode))
			.setFrontCounterClockwise(m_Params.FrontCounterClockwise);

		nvrhi::BlendState::RenderTarget renderTarget = nvrhi::BlendState::RenderTarget()
			.setBlendEnable(true)
			.setColorWriteMask(nvrhi::ColorMask::All)
			.setBlendOp(nvrhi::BlendOp::Add)
			.setBlendOpAlpha(nvrhi::BlendOp::Add)
			.setSrcBlend(nvrhi::BlendFactor::SrcAlpha)
			.setSrcBlendAlpha(nvrhi::BlendFactor::One)
			.setDestBlend(nvrhi::BlendFactor::OneMinusSrcAlpha)
			.setDestBlendAlpha(nvrhi::BlendFactor::Zero);

		nvrhi::BlendState blendState = nvrhi::BlendState()
			.setAlphaToCoverageEnable(false)
			.setRenderTarget(0, renderTarget);

		const bool hasDepth = static_cast<NvrhiFramebuffer*>(m_Params.Framebuffer)->m_FramebufferHandle->getFramebufferInfo().depthFormat != nvrhi::Format::UNKNOWN;

		nvrhi::DepthStencilState depthStencilState = nvrhi::DepthStencilState()
			.setDepthTestEnable(hasDepth)
			.setDepthWriteEnable(hasDepth)
			.setDepthFunc(hasDepth ? nvrhi::ComparisonFunc::Less : nvrhi::ComparisonFunc::Always);

		nvrhi::RenderState renderState = nvrhi::RenderState()
			.setRasterState(rasterState)
			.setBlendState(blendState)
			.setDepthStencilState(depthStencilState);

		const auto& vsHandle = nvrhiShader->m_ShaderHandles[ShaderType::Vertex];
		const auto& psHandle = nvrhiShader->m_ShaderHandles[ShaderType::Pixel];
		if (!vsHandle) DE_CORE_ERROR("Pipeline '{}': vertex shader handle is null — DXBC/SPIR-V compilation failed.", m_Params.DebugName);
		if (!psHandle) DE_CORE_ERROR("Pipeline '{}': pixel shader handle is null — DXBC/SPIR-V compilation failed.", m_Params.DebugName);
		DE_CORE_ASSERT(vsHandle && psHandle, "Shader compilation failed — see errors above.");

		nvrhi::GraphicsPipelineDesc graphicsPipelineDesc = nvrhi::GraphicsPipelineDesc()
			.setPrimType(nvrhi::PrimitiveType::TriangleList)
			.setRenderState(renderState)
			.setInputLayout(m_InputLayoutHandle)
			.setVertexShader(vsHandle)
			.setPixelShader(psHandle);

		if (nvrhiShader->m_BindingLayoutHandle)
		{
			graphicsPipelineDesc.addBindingLayout(nvrhiShader->m_BindingLayoutHandle);
		}

		m_GraphicsPipelineHandle = device->createGraphicsPipeline(graphicsPipelineDesc, static_cast<NvrhiFramebuffer*>(m_Params.Framebuffer)->m_FramebufferHandle);
		if (!m_GraphicsPipelineHandle) DE_CORE_ERROR("Pipeline '{}': createGraphicsPipeline failed — check D3D12 debug output for root signature or PSO errors.", m_Params.DebugName);
		DE_CORE_ASSERT(m_GraphicsPipelineHandle, "createGraphicsPipeline returned null — see errors above.");
	}

	void NvrhiPipeline::Destroy()
	{
		m_BindingSetHandle = nullptr;
		m_InputLayoutHandle = nullptr;
		m_GraphicsPipelineHandle = nullptr;
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
				.setFormat((nvrhi::Format)attribute.Format)
				.setOffset(attribute.Offset)
				.setElementStride(m_Params.VertexLayout.Stride);

			attributes.push_back(vertexAttributeDesc);

			//index++;
		}

		m_InputLayoutHandle = device->createInputLayout(attributes.data(), static_cast<uint32_t>(attributes.size()), nvrhiShader->m_ShaderHandles[ShaderType::Vertex]);
	}

	void NvrhiPipeline::CreateBindingSet(nvrhi::BindingLayoutHandle bindingLayoutHandle)
	{
		if (m_Params.UniformBuffer == nullptr && m_Params.Texture == nullptr)
		{
			return;
		}

		nvrhi::BindingSetDesc bindingSetDesc = nvrhi::BindingSetDesc();

		if (m_Params.UniformBuffer)
		{
			bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, static_cast<NvrhiGraphicsBuffer*>(m_Params.UniformBuffer)->m_BufferHandle));
		}

		if (m_Params.Texture)
		{
			bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, static_cast<NvrhiTexture*>(m_Params.Texture)->m_Handle));
			bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(2, static_cast<NvrhiSampler*>(Renderer::GetClampSampler())->m_Handle));
		}

		m_BindingSetHandle = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createBindingSet(bindingSetDesc, bindingLayoutHandle);
	}

}
