#pragma once
#include "DingoEngine/Graphics/Enums/SamplerAddressMode.h"

namespace Dingo
{

	struct SamplerParams
	{
		SamplerAddressMode AddressMode = SamplerAddressMode::Clamp;
		bool MinFilter = true;
		bool MagFilter = true;
		bool MipFilter = true;

		SamplerParams& SetAddressMode(SamplerAddressMode mode)
		{
			AddressMode = mode;
			return *this;
		}

		SamplerParams& SetMinFilter(bool enabled)
		{
			MinFilter = enabled;
			return *this;
		}

		SamplerParams& SetMagFilter(bool enabled)
		{
			MagFilter = enabled;
			return *this;
		}

		SamplerParams& SetMipFilter(bool enabled)
		{
			MipFilter = enabled;
			return *this;
		}
	};

	class Sampler
	{
	public:
		static Sampler* Create(const SamplerParams& params);

	public:
		Sampler(const SamplerParams& params)
			: m_Params(params)
		{
		}
		virtual ~Sampler() = default;

	public:
		virtual void Initialize() = 0;
		virtual void Destroy() = 0;

		virtual const SamplerParams& GetParams() const { return m_Params; }

	protected:
		SamplerParams m_Params;
	};

}
