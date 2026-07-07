#pragma once
#include "DingoEngine/Graphics/RenderPass.h"
#include "DingoEngine/Graphics/IBindableShaderResource.h"

#include <nvrhi/nvrhi.h>

namespace Dingo
{

	class NvrhiRenderPass : public RenderPass
	{
	public:
		NvrhiRenderPass(const RenderPassParams& params)
			: RenderPass(params)
		{}
		virtual ~NvrhiRenderPass() = default;

	public:
		virtual void Initialize() override;
		virtual void Destroy() override;

		virtual void SetUniformBuffer(uint32_t slot, GraphicsBuffer* buffer) override;

		virtual void SetTexture(uint32_t slot, Texture* texture, uint32_t arrayElement = 0) override;

		virtual void SetSampler(uint32_t slot, Sampler* sampler) override;

		virtual void Bake() override;

	private:
		bool m_Valid = false;

		nvrhi::BindingSetDesc m_BindingSetDesc;
		nvrhi::BindingSetHandle m_BindingSetHandle;
		// Shader generation the binding set was baked against; a hot-reload replaces
		// the shader's binding layout, so a mismatch forces a re-bake at bind time.
		uint32_t m_BuiltShaderGeneration = 0;

		friend class NvrhiCommandList; // Allow CommandList to access private members
	};

}
