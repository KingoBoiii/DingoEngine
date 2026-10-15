#pragma once
#include "DingoEngine/Graphics/Texture.h"

#include <nvrhi/nvrhi.h>

namespace Dingo
{

	// Every D3D depth target is created with this as its optimized clear value and every depth clear
	// uses it: D3D12 warns on, and can't fast-clear, a clear that differs from the created value.
	inline constexpr float k_DepthClearValue = 1.0f;

	class NvrhiTexture : public Texture
	{
	public:
		NvrhiTexture(const TextureParams& params)
			: Texture(params)
		{}
		virtual ~NvrhiTexture();

	public:
		virtual void Initialize() override;
		virtual void Destroy() override;
		virtual void Upload(const void* data) override;
		virtual void Reinitialize(const TextureParams& params) override;
		virtual void ReadPixels(std::function<void(const TexturePixels&)> done) override;

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
		friend class NvrhiComputePass;
		friend class NvrhiCommandList;
	};

}
