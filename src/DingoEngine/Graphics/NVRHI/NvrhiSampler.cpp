#include "depch.h"
#include "NvrhiSampler.h"
#include "NvrhiGraphicsContext.h"


namespace Dingo
{

	namespace Utils
	{

		static nvrhi::SamplerAddressMode GetSamplerAddressMode(const SamplerAddressMode addressMode)
		{
			switch (addressMode)
			{
				case SamplerAddressMode::Repeat: return nvrhi::SamplerAddressMode::Repeat;
				case SamplerAddressMode::MirroredRepeat: return nvrhi::SamplerAddressMode::MirroredRepeat;
				case SamplerAddressMode::ClampToEdge: return nvrhi::SamplerAddressMode::ClampToEdge;
				case SamplerAddressMode::ClampToBorder: return nvrhi::SamplerAddressMode::ClampToBorder;
				case SamplerAddressMode::MirrorClampToEdge: return nvrhi::SamplerAddressMode::MirrorClampToEdge;
				default: break;
			}
			return nvrhi::SamplerAddressMode::ClampToEdge; // Default to ClampToEdge if unknown
		}

	}

	void NvrhiSampler::Initialize()
	{
		nvrhi::SamplerDesc samplerDesc = nvrhi::SamplerDesc()
			.setAllAddressModes(Utils::GetSamplerAddressMode(m_Params.AddressMode))
			.setMinFilter(m_Params.MinFilter)
			.setMagFilter(m_Params.MagFilter)
			.setMipFilter(m_Params.MipFilter);

		m_Handle = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createSampler(samplerDesc);
	}

	void NvrhiSampler::Destroy()
	{
		if (m_Handle)
		{
			m_Handle->Release();
		}
	}

}
