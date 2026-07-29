#pragma once
#include "DingoEngine/Graphics/Texture.h"

#include <nvrhi/nvrhi.h>

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
		virtual void Reinitialize(const TextureParams& params) override;

		virtual bool NativeEquals(const Texture* other) const override
		{
			// Compared through the native handle rather than a dynamic_cast to NvrhiTexture:
			// this runs once per occupied texture slot for every textured quad, and RTTI
			// traversal there cost ~20-50 ns a pop. Two textures from different backends
			// cannot share a native pointer, so the cast bought no safety.
			return other && GetTextureHandle() == other->GetTextureHandle();
		}

		virtual void* GetTextureHandle() const override { return static_cast<void*>(m_Handle.Get()); }
		virtual const void* GetResourceHandle() const override { return GetTextureHandle(); }

	private:
		nvrhi::TextureHandle m_Handle;

		friend class NvrhiPipeline;
		friend class NvrhiRenderPass;
	};

}
