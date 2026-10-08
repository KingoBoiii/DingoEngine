#pragma once
#include "DingoEngine/Graphics/ComputePass.h"

#include <nvrhi/nvrhi.h>

#include <vector>

namespace Dingo
{

	class NvrhiComputePass : public ComputePass
	{
	public:
		NvrhiComputePass(const ComputePassParams& params)
			: ComputePass(params)
		{}
		virtual ~NvrhiComputePass() = default;

	public:
		virtual void Initialize() override;
		virtual void Destroy() override;

		virtual void SetUniformBuffer(uint32_t binding, GraphicsBuffer* buffer) override;
		virtual void SetStorageBuffer(uint32_t binding, GraphicsBuffer* buffer) override;
		virtual void SetTexture(uint32_t binding, Texture* texture) override;
		virtual void SetStorageTexture(uint32_t binding, Texture* texture) override;
		virtual void SetSampler(uint32_t binding, Sampler* sampler) override;

		// Builds what changed since the last dispatch; false when the pipeline or binding set can't be built.
		bool Prepare();

		nvrhi::ComputePipelineHandle GetPipelineHandle() const { return m_PipelineHandle; }
		nvrhi::BindingSetHandle GetBindingSetHandle() const { return m_BindingSetHandle; }
		const std::vector<nvrhi::BindingSetItem>& GetStorageItems() const { return m_StorageItems; }

	private:
		void SetItem(const nvrhi::BindingSetItem& item, Texture* texture = nullptr);

	private:
		struct Binding
		{
			nvrhi::BindingSetItem Item;
			Texture* Texture = nullptr; // re-pointed when it is reinitialized
			uint32_t Generation = 0;
		};
		std::vector<Binding> m_Bindings;
		std::vector<nvrhi::BindingSetItem> m_StorageItems; // of the built binding set

		nvrhi::ComputePipelineHandle m_PipelineHandle;
		nvrhi::BindingSetHandle m_BindingSetHandle;
		uint32_t m_BuiltShaderGeneration = ~0u;
		bool m_BindingsValid = false;
		bool m_FailureLogged = false;
	};

}
