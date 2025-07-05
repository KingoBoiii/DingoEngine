#pragma once
#include "DingoEngine/Graphics/Texture.h"

#include <nvrhi/nvrhi.h>

namespace DingoEngine
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

	private:
		nvrhi::TextureHandle m_Handle;

		friend class NvrhiPipeline;
	};

}
