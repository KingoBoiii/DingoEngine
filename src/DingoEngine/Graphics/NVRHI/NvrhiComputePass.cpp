#include "depch.h"
#include "NvrhiComputePass.h"
#include "NvrhiCommandList.h"
#include "NvrhiGraphicsBuffer.h"
#include "NvrhiGraphicsContext.h"
#include "NvrhiSampler.h"
#include "NvrhiShader.h"
#include "NvrhiTexture.h"

#include "../PushConstants.h"

#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Graphics/GraphicsDevice.h"

namespace Dingo
{

	ComputePass* ComputePass::Create(const ComputePassParams& params)
	{
		if (!Internal::RequireGraphicsDevice("ComputePass::Create"))
			return nullptr;

		DE_CORE_ASSERT(params.Shader, "A ComputePass needs a shader.");

		ComputePass* pass = new NvrhiComputePass(params);
		pass->Initialize();
		return pass;
	}

	void NvrhiComputePass::Initialize()
	{
		m_Bindings.clear();
		m_BindingsValid = false;
		m_BuiltShaderGeneration = ~0u;
	}

	void NvrhiComputePass::Destroy()
	{
		m_BindingSetHandle = nullptr;
		m_PipelineHandle = nullptr;
		m_Bindings.clear();
		m_StorageItems.clear();
	}

	void NvrhiComputePass::SetItem(const nvrhi::BindingSetItem& item, Texture* texture, uint32_t baseMip, uint32_t mipCount)
	{
		m_BindingsValid = false;
		m_InvalidLogged = false;
		Binding entry = { item, texture, texture ? texture->GetGeneration() : 0, baseMip, mipCount };
		if (texture)
			entry.Error = RefreshTexture(entry);
		for (Binding& binding : m_Bindings)
		{
			const bool sameKind = binding.Item.type == item.type || (NvrhiCommandList::IsRawBufferItem(binding.Item) && NvrhiCommandList::IsRawBufferItem(item));
			if (binding.Item.slot == item.slot && sameKind)
			{
				binding = entry;
				return;
			}
		}
		m_Bindings.push_back(entry);
	}

	void NvrhiComputePass::SetUniformBuffer(uint32_t binding, GraphicsBuffer* buffer)
	{
		DE_CORE_ASSERT(buffer && buffer->IsType(BufferType::UniformBuffer), "SetUniformBuffer takes a uniform buffer.");
		SetItem(nvrhi::BindingSetItem::ConstantBuffer(binding, static_cast<NvrhiGraphicsBuffer*>(buffer)->m_BufferHandle));
	}

	void NvrhiComputePass::SetStorageBuffer(uint32_t binding, GraphicsBuffer* buffer)
	{
		DE_CORE_ASSERT(buffer && buffer->IsType(BufferType::StorageBuffer), "SetStorageBuffer takes a storage buffer.");
		nvrhi::IBuffer* handle = static_cast<NvrhiGraphicsBuffer*>(buffer)->m_BufferHandle;
		const bool readOnly = m_Params.Shader->IsStorageBufferReadOnly(binding);
		SetItem(readOnly ? nvrhi::BindingSetItem::RawBuffer_SRV(binding, handle) : nvrhi::BindingSetItem::RawBuffer_UAV(binding, handle));
	}

	namespace
	{
		uint32_t ResolvedMipCount(const Texture* texture, uint32_t baseMip, uint32_t mipCount)
		{
			return mipCount ? mipCount : texture->GetParams().MipLevels - baseMip;
		}
	}

	std::string NvrhiComputePass::RefreshTexture(Binding& binding)
	{
		const Texture* texture = binding.Texture;
		const TextureParams& params = texture->GetParams();
		const bool isStorage = binding.Item.type == nvrhi::ResourceType::Texture_UAV;
		const uint32_t mipCount = isStorage ? 1 : binding.MipCount;

		binding.Item.resourceHandle = static_cast<const NvrhiTexture*>(texture)->m_Handle;
		binding.Item.dimension = nvrhi::TextureDimension::Unknown;

		if (binding.BaseMip >= params.MipLevels || (mipCount != 0 && mipCount > params.MipLevels - binding.BaseMip))
			return std::format("asks for mips {}+{} of a texture with {} mip levels", binding.BaseMip, mipCount, params.MipLevels);
		if (isStorage && binding.BaseMip > 0 && params.Dimension == TextureDimension::Texture3D)
			return std::format("asks for storage mip {} of a 3D texture; a 3D storage view is mip 0 only", binding.BaseMip);

		if (isStorage)
		{
			binding.Item.subresources = nvrhi::TextureSubresourceSet(binding.BaseMip, 1, 0, nvrhi::TextureSubresourceSet::AllArraySlices);
			if (params.Dimension == TextureDimension::TextureCube)
				binding.Item.dimension = nvrhi::TextureDimension::Texture2DArray;
		}
		else
		{
			binding.Item.subresources = nvrhi::TextureSubresourceSet(binding.BaseMip, ResolvedMipCount(texture, binding.BaseMip, binding.MipCount), 0, nvrhi::TextureSubresourceSet::AllArraySlices);
		}
		return {};
	}

	bool NvrhiComputePass::ValidateBindings()
	{
		const char* problem = nullptr;
		std::string detail;
		for (const Binding& binding : m_Bindings)
		{
			if (!binding.Texture)
				continue;
			if (!binding.Error.empty())
			{
				detail = std::format("binding {} {}", binding.Item.slot, binding.Error);
				problem = "has an invalid texture binding";
				break;
			}
			if (binding.Item.type != nvrhi::ResourceType::Texture_UAV)
				continue;
			for (const Binding& sampled : m_Bindings)
			{
				if (sampled.Texture != binding.Texture || sampled.Item.type != nvrhi::ResourceType::Texture_SRV)
					continue;
				const uint32_t first = sampled.BaseMip;
				const uint32_t last = first + ResolvedMipCount(sampled.Texture, first, sampled.MipCount);
				if (binding.BaseMip >= first && binding.BaseMip < last)
				{
					detail = std::format("storage binding {} writes mip {} of a texture that binding {} samples (mips {}-{}); the ranges must be disjoint", binding.Item.slot, binding.BaseMip, sampled.Item.slot, first, last - 1);
					problem = "binds one texture as sampled and storage over the same mip";
					break;
				}
			}
			if (problem)
				break;
		}
		if (!problem)
			return true;
		if (!m_InvalidLogged)
		{
			DE_CORE_ERROR("ComputePass '{}': {}: {}; dispatch skipped.", m_Params.DebugName, problem, detail);
			m_InvalidLogged = true;
		}
		return false;
	}

	void NvrhiComputePass::SetTexture(uint32_t binding, Texture* texture, uint32_t baseMip, uint32_t mipCount)
	{
		DE_CORE_ASSERT(texture, "Texture must not be null.");
		SetItem(nvrhi::BindingSetItem::Texture_SRV(binding, static_cast<NvrhiTexture*>(texture)->m_Handle), texture, baseMip, mipCount);
	}

	void NvrhiComputePass::SetStorageTexture(uint32_t binding, Texture* texture, uint32_t mipLevel)
	{
		DE_CORE_ASSERT(texture && texture->GetParams().IsStorage, "SetStorageTexture takes a texture made with TextureParams::IsStorage.");
		SetItem(nvrhi::BindingSetItem::Texture_UAV(binding, static_cast<NvrhiTexture*>(texture)->m_Handle), texture, mipLevel, 1);
	}

	void NvrhiComputePass::SetSampler(uint32_t binding, Sampler* sampler)
	{
		DE_CORE_ASSERT(sampler, "Sampler must not be null.");
		SetItem(nvrhi::BindingSetItem::Sampler(binding, static_cast<NvrhiSampler*>(sampler)->m_Handle));
	}

	bool ComputePass::SetPushConstants(const void* data, uint32_t size)
	{
		return Internal::CopyPushConstants(m_Params.Shader, data, size, m_PushConstants, "ComputePass", m_Params.DebugName);
	}

	bool NvrhiComputePass::Prepare()
	{
		NvrhiShader* shader = static_cast<NvrhiShader*>(m_Params.Shader);
		nvrhi::IDevice* device = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle();

		if (shader->GetGeneration() != m_BuiltShaderGeneration)
		{
			m_PipelineHandle = nullptr;
			m_BindingsValid = false;
			const auto stage = shader->m_ShaderHandles.find(ShaderType::Compute);
			if (stage == shader->m_ShaderHandles.end() || !stage->second)
			{
				if (!m_FailureLogged)
				{
					DE_CORE_ERROR("ComputePass '{}': shader '{}' has no compute stage.", m_Params.DebugName, shader->GetParams().Name);
					m_FailureLogged = true;
				}
				return false;
			}

			nvrhi::ComputePipelineDesc desc = nvrhi::ComputePipelineDesc().setComputeShader(stage->second);
			if (shader->m_BindingLayoutHandle)
				desc.addBindingLayout(shader->m_BindingLayoutHandle);
			m_PipelineHandle = device->createComputePipeline(desc);
			m_BuiltShaderGeneration = shader->GetGeneration();
		}
		if (!m_PipelineHandle)
			return false;

		for (Binding& binding : m_Bindings)
		{
			if (binding.Texture && binding.Texture->GetGeneration() != binding.Generation)
			{
				binding.Generation = binding.Texture->GetGeneration();
				binding.Error = RefreshTexture(binding);
				m_BindingsValid = false;
			}
		}

		if (!m_BindingsValid && !ValidateBindings())
			return false;

		if (!m_BindingsValid)
		{
			m_BindingSetHandle = nullptr;
			m_StorageItems.clear();
			// A hot-reload can add or drop a block's readonly: the item follows the shader as it is now.
			for (Binding& binding : m_Bindings)
			{
				if (NvrhiCommandList::IsRawBufferItem(binding.Item))
					binding.Item.type = shader->IsStorageBufferReadOnly(binding.Item.slot) ? nvrhi::ResourceType::RawBuffer_SRV : nvrhi::ResourceType::RawBuffer_UAV;
			}
			if ((!m_Bindings.empty() || shader->m_PushConstantSize) && shader->m_BindingLayoutHandle)
			{
				nvrhi::BindingSetDesc desc;
				for (const Binding& binding : m_Bindings)
					desc.addItem(binding.Item);
				// D3D11 binds its push-constant buffer through the binding set.
				if (shader->m_PushConstantSize)
					desc.addItem(nvrhi::BindingSetItem::PushConstants(shader->m_PushConstantRegister, shader->m_PushConstantSize));
				NvrhiCommandList::SortBySlot(desc);
				m_BindingSetHandle = device->createBindingSet(desc, shader->m_BindingLayoutHandle);
				if (!m_BindingSetHandle)
				{
					if (!m_FailureLogged)
					{
						DE_CORE_ERROR("ComputePass '{}': the bindings don't match shader '{}'s layout.", m_Params.DebugName, shader->GetParams().Name);
						m_FailureLogged = true;
					}
					return false;
				}
				for (const Binding& binding : m_Bindings)
				{
					if (NvrhiCommandList::IsStorageItem(binding.Item) || binding.Item.type == nvrhi::ResourceType::Texture_SRV)
						m_StorageItems.push_back(binding.Item);
				}
			}
			m_BindingsValid = true;
		}
		return true;
	}

}
