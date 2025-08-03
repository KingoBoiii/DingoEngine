#pragma once
#include "DingoEngine/Graphics/Texture.h"

#include <nvrhi/nvrhi.h>

//#define ENABLE_TEXTURE_SAMPLER

namespace Dingo
{

	class NvrhiTexture : public Texture
	{
	public:
		NvrhiTexture(const TextureParams& params)
			: Texture(params)
		{}
		virtual ~NvrhiTexture() = default;

	public:
		virtual void Initialize() override;
		virtual void Destroy() override;
		virtual void Upload(const void* data, uint64_t size) override;
		virtual void Upload(const std::filesystem::path& filepath) override;

		virtual void* GetTextureHandle() const override { return static_cast<void*>(m_Handle.Get()); }

	private:
		nvrhi::TextureHandle m_Handle;
#ifdef ENABLE_TEXTURE_SAMPLER
		nvrhi::SamplerHandle m_SamplerHandle;
#endif

		friend class NvrhiPipeline;
		friend class NvrhiRenderPass;
	};

}
