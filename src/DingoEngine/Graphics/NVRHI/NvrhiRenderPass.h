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
		void RecordTextureBinding(uint32_t slot, Texture* texture, uint32_t arrayElement);
		// Re-points any binding whose Texture was reinitialized (hot-reload) at its new
		// native handle and clears m_Valid. Returns true when something changed.
		bool RefreshReloadedTextures();

	private:
		bool m_Valid = false;

		nvrhi::BindingSetDesc m_BindingSetDesc;
		nvrhi::BindingSetHandle m_BindingSetHandle;
		// Shader generation the binding set was baked against; a hot-reload replaces
		// the shader's binding layout, so a mismatch forces a re-bake at bind time.
		uint32_t m_BuiltShaderGeneration = 0;

		// Every texture bound through SetTexture, with the generation its handle came
		// from. Owners that bake once never re-set their slots, so this is the only way a
		// reload can reach an already-baked binding set.
		struct TextureBinding
		{
			uint32_t Slot = 0;
			uint32_t ArrayElement = 0;
			Texture* Texture = nullptr;
			uint32_t Generation = 0;
		};
		std::vector<TextureBinding> m_TextureBindings;

		friend class NvrhiCommandList; // Allow CommandList to access private members
	};

}
