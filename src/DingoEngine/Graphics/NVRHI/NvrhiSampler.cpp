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
			.setAddressU(Utils::GetSamplerAddressMode(m_Params.AddressU))
			.setAddressV(Utils::GetSamplerAddressMode(m_Params.AddressV))
			.setAddressW(Utils::GetSamplerAddressMode(m_Params.AddressW))
			.setMinFilter(m_Params.MinFilter)
			.setMagFilter(m_Params.MagFilter)
			.setMipFilter(m_Params.MipFilter)
			.setBorderColor(nvrhi::Color(m_Params.BorderColor.r, m_Params.BorderColor.g, m_Params.BorderColor.b, m_Params.BorderColor.a))
			.setMaxAnisotropy(m_Params.MaxAnisotropy)
			.setReductionType(m_Params.Compare ? nvrhi::SamplerReductionType::Comparison : nvrhi::SamplerReductionType::Standard);

		m_Handle = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createSampler(samplerDesc);
	}

	void NvrhiSampler::Destroy()
	{
		m_Handle = nullptr;
	}

}
