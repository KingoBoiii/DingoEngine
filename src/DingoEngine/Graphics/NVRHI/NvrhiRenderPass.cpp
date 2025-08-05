#include "depch.h"
#include "NvrhiRenderPass.h"
#include "NvrhiGraphicsContext.h"
#include "NvrhiGraphicsBuffer.h"
#include "NvrhiShader.h"
#include "NvrhiTexture.h"
#include "NvrhiSampler.h"

namespace Dingo
{

	void NvrhiRenderPass::Initialize()
	{
		DE_CORE_ASSERT(m_Params.Pipeline, "RenderPass must have a valid Pipeline set before initialization.");

		m_BindingSetDesc = nvrhi::BindingSetDesc();
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

		m_Resources.push_back(RenderPassInput{
			.Slot = slot,
			.ArrayElement = 0,
			.Type = RenderPassResourceType::UniformBuffer,
			.Handle = buffer
			});

		m_Valid = false;

		//m_BindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(slot, static_cast<NvrhiGraphicsBuffer*>(buffer)->m_BufferHandle));
	}

	void NvrhiRenderPass::SetTexture(uint32_t slot, Texture* texture, uint32_t arrayElement)
	{
		DE_CORE_ASSERT(texture, "Texture must not be null.");

		// search if the texture is already in resources
		for (auto& resource : m_Resources)
		{
			if (resource.Slot == slot &&
				resource.Type == RenderPassResourceType::Texture &&
				resource.ArrayElement == arrayElement)
			{
				resource.Handle = texture;
				m_Valid = false;
				return;
			}
		}

		m_Resources.push_back(RenderPassInput{
			.Slot = slot,
			.ArrayElement = arrayElement,
			.Type = RenderPassResourceType::Texture,
			.Handle = texture
			});

		m_Valid = false;

		// search if the texture is already in the binding set
#if 0
		static uint32_t c = 0;
		if (c > 3)
		{
			for (auto& item : m_BindingSetDesc.bindings)
			{
				if (item.slot == slot && item.type == nvrhi::ResourceType::Texture_SRV)
				{
					// If the texture is already set, we can skip adding it again
					if (item.resourceHandle == static_cast<NvrhiTexture*>(texture)->m_Handle)
					{
						item.resourceHandle = static_cast<NvrhiTexture*>(texture)->m_Handle;
						return;
					}
				}
			}
		}
		c++;

		m_BindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(slot, static_cast<NvrhiTexture*>(texture)->m_Handle).setArrayElement(arrayElement));
#ifdef ENABLE_TEXTURE_SAMPLER
		m_BindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(slot + 1, static_cast<NvrhiTexture*>(texture)->m_SamplerHandle).setArrayElement(arrayElement));
#endif
#endif
	}

	void NvrhiRenderPass::SetSampler(uint32_t slot, Sampler* sampler)
	{
		DE_CORE_ASSERT(sampler, "Sampler must not be null.");

		m_Resources.push_back(RenderPassInput{
			.Slot = slot,
			.ArrayElement = 0,
			.Type = RenderPassResourceType::Sampler,
			.Handle = sampler
			});

		m_Valid = false;
		//m_BindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(slot, static_cast<NvrhiSampler*>(sampler)->m_Handle));
	}

	void NvrhiRenderPass::Bake()
	{
		if (m_Valid)
		{
			return;
		}

		nvrhi::BindingSetDesc bindingSetDesc = nvrhi::BindingSetDesc();

		for (const auto& input : m_Resources)
		{
			switch (input.Type)
			{
				case RenderPassResourceType::UniformBuffer:
					bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(input.Slot, (nvrhi::IBuffer*)input.Handle->GetResourceHandle()));
					break;
				case RenderPassResourceType::Texture:
					bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(input.Slot, (nvrhi::ITexture*)input.Handle->GetResourceHandle()).setArrayElement(input.ArrayElement));
					break;
				case RenderPassResourceType::Sampler:
					bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(input.Slot, (nvrhi::ISampler*)input.Handle->GetResourceHandle()));
					break;

				default:
					break;
			}
		}

		m_BindingSetHandle = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createBindingSet(bindingSetDesc, static_cast<NvrhiShader*>(m_Params.Pipeline->GetParams().Shader)->m_BindingLayoutHandle);
		DE_CORE_ASSERT(m_BindingSetHandle, "Failed to create BindingSetHandle in NvrhiRenderPass::Bake");
	}

}
