#include "depch.h"
#include "NvrhiComputePass.h"
#include "NvrhiGraphicsBuffer.h"
#include "NvrhiGraphicsContext.h"
#include "NvrhiSampler.h"
#include "NvrhiShader.h"
#include "NvrhiTexture.h"

#include "DingoEngine/Graphics/GraphicsContext.h"

namespace Dingo
{

	ComputePass* ComputePass::Create(const ComputePassParams& params)
	{
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
	}

	void NvrhiComputePass::SetItem(const nvrhi::BindingSetItem& item, Texture* texture)
	{
		m_BindingsValid = false;
		for (Binding& binding : m_Bindings)
		{
			if (binding.Item.slot == item.slot && binding.Item.type == item.type)
			{
				binding = { item, texture, texture ? texture->GetGeneration() : 0 };
				return;
			}
		}
		m_Bindings.push_back({ item, texture, texture ? texture->GetGeneration() : 0 });
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

	void NvrhiComputePass::SetTexture(uint32_t binding, Texture* texture)
	{
		DE_CORE_ASSERT(texture, "Texture must not be null.");
		SetItem(nvrhi::BindingSetItem::Texture_SRV(binding, static_cast<NvrhiTexture*>(texture)->m_Handle), texture);
	}

	void NvrhiComputePass::SetStorageTexture(uint32_t binding, Texture* texture)
	{
		DE_CORE_ASSERT(texture && texture->GetParams().IsStorage, "SetStorageTexture takes a texture made with TextureParams::IsStorage.");
		SetItem(nvrhi::BindingSetItem::Texture_UAV(binding, static_cast<NvrhiTexture*>(texture)->m_Handle), texture);
	}

	void NvrhiComputePass::SetSampler(uint32_t binding, Sampler* sampler)
	{
		DE_CORE_ASSERT(sampler, "Sampler must not be null.");
		SetItem(nvrhi::BindingSetItem::Sampler(binding, static_cast<NvrhiSampler*>(sampler)->m_Handle));
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
				binding.Item.resourceHandle = static_cast<NvrhiTexture*>(binding.Texture)->m_Handle;
				binding.Generation = binding.Texture->GetGeneration();
				m_BindingsValid = false;
			}
		}

		if (!m_BindingsValid)
		{
			m_BindingSetHandle = nullptr;
			if (!m_Bindings.empty() && shader->m_BindingLayoutHandle)
			{
				nvrhi::BindingSetDesc desc;
				for (const Binding& binding : m_Bindings)
					desc.addItem(binding.Item);
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
			}
			m_BindingsValid = true;
		}
		return true;
	}

}
