#pragma once
#include "DingoEngine/Graphics/Enums/SamplerAddressMode.h"
#include "DingoEngine/Graphics/IBindableShaderResource.h"

#include <glm/glm.hpp>

namespace Dingo
{

	struct SamplerParams
	{
		SamplerAddressMode AddressU = SamplerAddressMode::Clamp;
		SamplerAddressMode AddressV = SamplerAddressMode::Clamp;
		SamplerAddressMode AddressW = SamplerAddressMode::Clamp;
		bool MinFilter = true;
		bool MagFilter = true;
		bool MipFilter = true;
		// Read outside the texture with SamplerAddressMode::Border. Vulkan has no free border colour:
		// it takes transparent black, opaque black or opaque white, whichever is nearest.
		glm::vec4 BorderColor{ 1.0f, 1.0f, 1.0f, 1.0f };
		// A depth comparison sampler (a GLSL sampler2DShadow, HLSL SampleCmp): a sample is 1 where the
		// reference depth is less than the stored one, else 0, filtered over the four texels for
		// hardware 2x2 PCF.
		bool Compare = false;
		float MaxAnisotropy = 1.0f;

		// Every axis at once.
		SamplerParams& SetAddressMode(SamplerAddressMode mode)
		{
			AddressU = AddressV = AddressW = mode;
			return *this;
		}

		SamplerParams& SetAddressModes(SamplerAddressMode u, SamplerAddressMode v, SamplerAddressMode w = SamplerAddressMode::Clamp)
		{
			AddressU = u;
			AddressV = v;
			AddressW = w;
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

		SamplerParams& SetBorderColor(const glm::vec4& color)
		{
			BorderColor = color;
			return *this;
		}

		SamplerParams& SetCompare(bool compare)
		{
			Compare = compare;
			return *this;
		}

		SamplerParams& SetMaxAnisotropy(float maxAnisotropy)
		{
			MaxAnisotropy = maxAnisotropy;
			return *this;
		}
	};

	class Sampler : public IBindableShaderResource
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
		// Create() has already initialized it.
		virtual void Initialize() = 0;
		virtual void Destroy() = 0;

		virtual const SamplerParams& GetParams() const { return m_Params; }

	protected:
		SamplerParams m_Params;
	};

}
