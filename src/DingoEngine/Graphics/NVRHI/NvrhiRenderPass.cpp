#include "depch.h"
#include "NvrhiRenderPass.h"
#include "NvrhiCommandList.h"
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

		// Every TextureBinding holds an index into the desc that was just emptied. Clearing
		// these with it is what keeps that index meaningful — the linear scan this replaced
		// re-derived the index each time and so could not go stale.
		m_TextureBindings.clear();
		m_TextureBindingIndices.clear();
		m_Valid = false;
	}

	void NvrhiRenderPass::Destroy()
	{
		m_BindingSetHandle = nullptr;
		m_StorageItems.clear();
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

		auto it = m_TextureBindingIndices.find(MakeTextureKey(slot, arrayElement));
		if (it != m_TextureBindingIndices.end())
		{
			TextureBinding& binding = m_TextureBindings[it->second];
			binding.Texture = texture;
			binding.Generation = texture->GetGeneration();

			// Only invalidate on a real change — Renderer2D re-sets all 32 slots
			// every flush, and an unconditional invalidate forced a fresh
			// createBindingSet per batch per frame.
			nvrhi::BindingSetItem& item = m_BindingSetDesc.bindings[binding.ItemIndex];
			if (item.resourceHandle != handle)
			{
				item.resourceHandle = handle;
				m_Valid = false;
			}

			return;
		}

		m_BindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(slot, handle).setArrayElement(arrayElement));

		m_TextureBindingIndices[MakeTextureKey(slot, arrayElement)] = m_TextureBindings.size();
		m_TextureBindings.push_back({ slot, arrayElement, texture, texture->GetGeneration(), m_BindingSetDesc.bindings.size() - 1 });
		m_Valid = false;
	}

	bool NvrhiRenderPass::RefreshReloadedTextures()
	{
		bool changed = false;

		for (TextureBinding& binding : m_TextureBindings)
		{
			if (binding.Texture->GetGeneration() == binding.Generation)
				continue;

			m_BindingSetDesc.bindings[binding.ItemIndex].resourceHandle = static_cast<NvrhiTexture*>(binding.Texture)->m_Handle;
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

	void NvrhiRenderPass::SetStorageBuffer(uint32_t slot, GraphicsBuffer* buffer)
	{
		DE_CORE_ASSERT(buffer && buffer->IsType(BufferType::StorageBuffer), "SetStorageBuffer takes a storage buffer.");

		nvrhi::IBuffer* handle = static_cast<NvrhiGraphicsBuffer*>(buffer)->m_BufferHandle;
		const Shader* shader = m_Params.Pipeline->GetParams().Shader;
		const bool readOnly = !shader || shader->IsStorageBufferReadOnly(slot);
		const nvrhi::BindingSetItem item = readOnly ? nvrhi::BindingSetItem::RawBuffer_SRV(slot, handle) : nvrhi::BindingSetItem::RawBuffer_UAV(slot, handle);
		for (nvrhi::BindingSetItem& existing : m_BindingSetDesc.bindings)
		{
			if (existing.slot == slot && NvrhiCommandList::IsRawBufferItem(existing))
			{
				if (existing.type != item.type)
				{
					existing.type = item.type;
					m_Valid = false;
				}
				if (existing.resourceHandle != handle)
				{
					existing.resourceHandle = handle;
					m_Valid = false;
				}
				return;
			}
		}
		m_BindingSetDesc.addItem(item);
		m_Valid = false;
	}

	void NvrhiRenderPass::Bake()
	{
		if (m_Valid)
		{
			return;
		}

		Shader* shader = m_Params.Pipeline->GetParams().Shader;

		// A hot-reload can add or drop a block's readonly, which moves it between a shader-resource and
		// an unordered-access view: the item follows the shader as it is now.
		for (nvrhi::BindingSetItem& item : m_BindingSetDesc.bindings)
		{
			if (shader && NvrhiCommandList::IsRawBufferItem(item))
				item.type = shader->IsStorageBufferReadOnly(item.slot) ? nvrhi::ResourceType::RawBuffer_SRV : nvrhi::ResourceType::RawBuffer_UAV;
		}

		m_BindingSetHandle = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createBindingSet(m_BindingSetDesc, static_cast<NvrhiShader*>(shader)->m_BindingLayoutHandle);

		if (!m_BindingSetHandle && !m_BindingSetDesc.bindings.empty())
		{
			// Stay invalid rather than latching a null binding set for the rest of the run:
			// the usual cause is a desc that no longer matches the shader's binding layout,
			// which the owner can still fix by re-setting its bindings.
			DE_CORE_ERROR("RenderPass::Bake: createBindingSet failed for shader '{}' — the binding set does not match the shader's binding layout.", shader->GetParams().Name);
			return;
		}

		m_StorageItems.clear();
		for (const nvrhi::BindingSetItem& item : m_BindingSetDesc.bindings)
		{
			if (NvrhiCommandList::IsStorageItem(item))
				m_StorageItems.push_back(item);
		}

		m_BuiltShaderGeneration = shader->GetGeneration();
		m_Valid = true;
	}

}
