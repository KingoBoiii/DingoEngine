#include "depch.h"
#include "DingoEngine/Graphics/Pipeline.h"
#include "DingoEngine/Graphics/GraphicsContext.h"

namespace DingoEngine
{

	Pipeline* Pipeline::Create(Shader* shader, Framebuffer* framebuffer)
	{
		return new Pipeline(shader, framebuffer);
	}

	Pipeline::Pipeline(Shader* shader, Framebuffer* framebuffer)
		: m_Shader(shader), m_Framebuffer(framebuffer)
	{}

	void Pipeline::Initialize()
	{
		const auto device = GraphicsContext::GetDeviceHandle();

		m_InputLayoutHandle = device->createInputLayout(nullptr, 0, m_Shader->m_VertexShaderHandle);

		nvrhi::GraphicsPipelineDesc graphicsPipelineDesc = nvrhi::GraphicsPipelineDesc()
			.setInputLayout(m_InputLayoutHandle)
			.setVertexShader(m_Shader->m_VertexShaderHandle)
			.setPixelShader(m_Shader->m_FragmentShaderHandle);

		m_GraphicsPipelineHandle = device->createGraphicsPipeline(graphicsPipelineDesc, m_Framebuffer->m_FramebufferHandle);
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

}
