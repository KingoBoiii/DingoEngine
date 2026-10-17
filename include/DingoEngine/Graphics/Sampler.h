#pragma once
#include "DingoEngine/Graphics/Enums/SamplerAddressMode.h"
#include "DingoEngine/Graphics/Enums/SamplerFilter.h"
#include "DingoEngine/Graphics/IBindableShaderResource.h"

#include <glm/glm.hpp>

namespace Dingo
{

	/// Describes how shaders read a texture, for Sampler::Create.
	struct SamplerParams
	{
		/// Addressing along the texture's U (x) axis.
		SamplerAddressMode AddressU = SamplerAddressMode::Clamp;
		/// Addressing along the texture's V (y) axis.
		SamplerAddressMode AddressV = SamplerAddressMode::Clamp;
		/// Addressing along the texture's W (z) axis.
		SamplerAddressMode AddressW = SamplerAddressMode::Clamp;
		SamplerFilter MinFilter = SamplerFilter::Linear; ///< Filter when the texture is minified.
		SamplerFilter MagFilter = SamplerFilter::Linear; ///< Filter when the texture is magnified.
		SamplerFilter MipFilter = SamplerFilter::Linear; ///< Filter between mip levels.
		/// Read outside the texture with SamplerAddressMode::Border. Vulkan has no free border
		/// colour: it takes transparent black, opaque black or opaque white, whichever is nearest.
		glm::vec4 BorderColor{ 1.0f, 1.0f, 1.0f, 1.0f };
		/// A depth comparison sampler (a GLSL sampler2DShadow, HLSL SampleCmp): a sample is 1 where
		/// the reference depth is less than the stored one, else 0, filtered over the four texels
		/// for hardware 2x2 PCF.
		bool Compare = false;
		float MaxAnisotropy = 1.0f; ///< Maximum anisotropic filtering samples; 1 turns it off.

		/// Sets AddressU, AddressV and AddressW to mode and returns *this.
		SamplerParams& SetAddressMode(SamplerAddressMode mode)
		{
			AddressU = AddressV = AddressW = mode;
			return *this;
		}

		/// Sets U, V and W separately and returns *this; w defaults to Clamp.
		SamplerParams& SetAddressModes(SamplerAddressMode u, SamplerAddressMode v, SamplerAddressMode w = SamplerAddressMode::Clamp)
		{
			AddressU = u;
			AddressV = v;
			AddressW = w;
			return *this;
		}

		/// Sets MinFilter, MagFilter and MipFilter to filter and returns *this.
		SamplerParams& SetFilter(SamplerFilter filter)
		{
			MinFilter = MagFilter = MipFilter = filter;
			return *this;
		}

		/// Each setter sets the field of the same name and returns *this, for chaining.
		///@{
		SamplerParams& SetMinFilter(SamplerFilter filter)
		{
			MinFilter = filter;
			return *this;
		}

		SamplerParams& SetMagFilter(SamplerFilter filter)
		{
			MagFilter = filter;
			return *this;
		}

		SamplerParams& SetMipFilter(SamplerFilter filter)
		{
			MipFilter = filter;
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
		///@}
	};

	/// A GPU sampler: how a shader addresses and filters a texture. The caller owns it; delete
	/// frees it.
	class Sampler : public IBindableShaderResource
	{
	public:
		/// Builds the sampler and returns it ready to bind. The caller owns the result. Returns nullptr
		/// (with an error) when there is no graphics device, as in a headless app.
		[[nodiscard]] static Sampler* Create(const SamplerParams& params);

	public:
		/// For subclasses; make samplers with Create.
		Sampler(const SamplerParams& params)
			: m_Params(params)
		{
		}
		/// Frees the GPU sampler.
		virtual ~Sampler() = default;

	protected:
		/// Create() calls it, so a resource is ready when the factory returns it.
		virtual void Initialize() = 0;

	public:
		/// Releases the GPU object early; the destructor releases everything anyway.
		virtual void Destroy() = 0;

		/// The params the sampler was created with.
		virtual const SamplerParams& GetParams() const { return m_Params; }
		/// Never reused, unlike the address, which a freed sampler can hand to a new one.
		uint64_t GetId() const { return m_Id; }

	private:
		static uint64_t AllocateId();

	protected:
		SamplerParams m_Params; ///< The params the sampler was created with.

	private:
		uint64_t m_Id = AllocateId();
	};

}
