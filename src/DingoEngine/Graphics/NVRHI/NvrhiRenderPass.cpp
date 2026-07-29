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
		m_BindingSetHandle = nullptr;
	}

	void NvrhiRenderPass::SetUniformBuffer(uint32_t slot, GraphicsBuffer* buffer)
	{
		DE_CORE_ASSERT(buffer, "Buffer must not be null.");
		DE_CORE_ASSERT(buffer->IsType(BufferType::UniformBuffer), "Graphics buffer, must be of type BufferType::UniformBuffer");

		m_BindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(slot, static_cast<NvrhiGraphicsBuffer*>(buffer)->m_BufferHandle));
		m_Valid = false;
	}

	void NvrhiRenderPass::SetTexture(uint32_t slot, Texture* texture, uint32_t arrayElement)
	{
		DE_CORE_ASSERT(texture, "Texture must not be null.");

		nvrhi::ITexture* handle = static_cast<NvrhiTexture*>(texture)->m_Handle;

		RecordTextureBinding(slot, texture, arrayElement);

		// search if the texture is already in the binding set
		for (auto& item : m_BindingSetDesc.bindings)
		{
			if (item.slot == slot &&
				item.type == nvrhi::ResourceType::Texture_SRV &&
				item.arrayElement == arrayElement)
			{
				// Only invalidate on a real change — Renderer2D re-sets all 32 slots
				// every flush, and an unconditional invalidate forced a fresh
				// createBindingSet per batch per frame.
				if (item.resourceHandle != handle)
				{
					item.resourceHandle = handle;
					m_Valid = false;
				}

				return;
			}
		}

		m_BindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(slot, handle).setArrayElement(arrayElement));
		m_Valid = false;
	}

	void NvrhiRenderPass::RecordTextureBinding(uint32_t slot, Texture* texture, uint32_t arrayElement)
	{
		for (TextureBinding& binding : m_TextureBindings)
		{
			if (binding.Slot == slot && binding.ArrayElement == arrayElement)
			{
				binding.Texture = texture;
				binding.Generation = texture->GetGeneration();
				return;
			}
		}

		m_TextureBindings.push_back({ slot, arrayElement, texture, texture->GetGeneration() });
	}

	bool NvrhiRenderPass::RefreshReloadedTextures()
	{
		bool changed = false;

		for (TextureBinding& binding : m_TextureBindings)
		{
			if (binding.Texture->GetGeneration() == binding.Generation)
				continue;

			nvrhi::ITexture* handle = static_cast<NvrhiTexture*>(binding.Texture)->m_Handle;

			for (auto& item : m_BindingSetDesc.bindings)
			{
				if (item.slot == binding.Slot &&
					item.type == nvrhi::ResourceType::Texture_SRV &&
					item.arrayElement == binding.ArrayElement)
				{
					item.resourceHandle = handle;
					break;
				}
			}

			binding.Generation = binding.Texture->GetGeneration();
			changed = true;
		}

		if (changed)
			m_Valid = false;

		return changed;
	}

	void NvrhiRenderPass::SetSampler(uint32_t slot, Sampler* sampler)
	{
		DE_CORE_ASSERT(sampler, "Sampler must not be null.");

		m_BindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(slot, static_cast<NvrhiSampler*>(sampler)->m_Handle));
		m_Valid = false;
	}

	void NvrhiRenderPass::Bake()
	{
		if (m_Valid)
		{
			return;
		}

		Shader* shader = m_Params.Pipeline->GetParams().Shader;
		m_BindingSetHandle = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createBindingSet(m_BindingSetDesc, static_cast<NvrhiShader*>(shader)->m_BindingLayoutHandle);

		if (!m_BindingSetHandle && !m_BindingSetDesc.bindings.empty())
		{
			// Stay invalid rather than latching a null binding set for the rest of the run:
			// the usual cause is a desc that no longer matches the shader's binding layout,
			// which the owner can still fix by re-setting its bindings.
			DE_CORE_ERROR("RenderPass::Bake: createBindingSet failed for shader '{}' — the binding set does not match the shader's binding layout.", shader->GetParams().Name);
			return;
		}

		m_BuiltShaderGeneration = shader->GetGeneration();
		m_Valid = true;
	}

}
