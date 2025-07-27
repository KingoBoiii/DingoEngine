#include "depch.h"
#include "NvrhiRenderPass.h"
#include "NvrhiGraphicsContext.h"
#include "NvrhiGraphicsBuffer.h"
#include "NvrhiShader.h"
#include "NvrhiTexture.h"

namespace Dingo
{

	void NvrhiRenderPass::Initialize()
	{
		DE_CORE_ASSERT(m_Params.Pipeline, "RenderPass must have a valid Pipeline set before initialization.");
	}

	void NvrhiRenderPass::Destroy()
	{
		if (m_BindingSetHandle)
		{
			m_BindingSetHandle->Release();
		}
	}

	void NvrhiRenderPass::SetUniformBuffer(uint32_t slot, GraphicsBuffer* buffer)
	{
		DE_CORE_ASSERT(buffer, "Buffer must not be null.");
		DE_CORE_ASSERT(buffer->IsType(BufferType::UniformBuffer), "Graphics buffer, must be of type BufferType::UniformBuffer");

		m_BindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(slot, static_cast<NvrhiGraphicsBuffer*>(buffer)->m_BufferHandle));
	}

	void NvrhiRenderPass::SetTexture(uint32_t slot, Texture* texture)
	{
		DE_CORE_ASSERT(texture, "Texture must not be null.");

		m_BindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(slot, static_cast<NvrhiTexture*>(texture)->m_Handle));
		m_BindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(slot + 1, static_cast<NvrhiTexture*>(texture)->m_SamplerHandle));
	}

	void NvrhiRenderPass::Bake()
	{
		m_BindingSetHandle = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createBindingSet(m_BindingSetDesc, static_cast<NvrhiShader*>(m_Params.Pipeline->GetParams().Shader)->m_BindingLayoutHandle);
		DE_CORE_ASSERT(m_BindingSetHandle, "Failed to create BindingSetHandle in NvrhiRenderPass::Bake");
	}

}
