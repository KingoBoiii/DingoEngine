#include "depch.h"
#include "DingoEngine/Graphics/Texture.h"
#include "DingoEngine/Asset/AssetPath.h"
#include "DingoEngine/Core/FileSystem.h"

#include "NVRHI/NvrhiTexture.h"

#include <atomic>
#include <cmath>
#include <cstring>

namespace Dingo
{

	namespace
	{
		float HalfToFloat(uint16_t half)
		{
			const uint32_t sign = (half >> 15) & 1u;
			const uint32_t exponent = (half >> 10) & 0x1Fu;
			const uint32_t mantissa = half & 0x3FFu;
			float value;
			if (exponent == 0)
				value = std::ldexp(static_cast<float>(mantissa), -24);
			else if (exponent == 31)
				value = mantissa ? NAN : INFINITY;
			else
				value = std::ldexp(static_cast<float>(mantissa | 0x400u), static_cast<int>(exponent) - 25);
			return sign ? -value : value;
		}

		// The unsigned small floats of R11G11B10F: 5 exponent bits and no sign, like a half's top bits.
		float SmallFloatToFloat(uint32_t bits, uint32_t mantissaBits)
		{
			const uint32_t exponent = bits >> mantissaBits;
			const uint32_t mantissa = bits & ((1u << mantissaBits) - 1u);
			if (exponent == 0)
				return std::ldexp(static_cast<float>(mantissa), -14 - static_cast<int>(mantissaBits));
			if (exponent == 31)
				return mantissa ? NAN : INFINITY;
			return std::ldexp(static_cast<float>(mantissa | (1u << mantissaBits)), static_cast<int>(exponent) - 15 - static_cast<int>(mantissaBits));
		}

		template<typename T>
		T ReadAt(const uint8_t* bytes, size_t index)
		{
			T value;
			std::memcpy(&value, bytes + index * sizeof(T), sizeof(T));
			return value;
		}
	}

	glm::vec4 TexturePixels::GetPixel(uint32_t x, uint32_t y) const
	{
		const uint32_t bytesPerPixel = GetBytesPerPixel(Format);
		if (x >= Width || y >= Height || bytesPerPixel == 0 || Data.size() < static_cast<size_t>(Width) * Height * bytesPerPixel)
			return glm::vec4(0.0f);

		const uint8_t* pixel = Data.data() + (static_cast<size_t>(y) * Width + x) * bytesPerPixel;
		switch (Format)
		{
			case TextureFormat::RGBA:
			case TextureFormat::RGBA8_UNORM:
				return glm::vec4(pixel[0], pixel[1], pixel[2], pixel[3]) / 255.0f;
			case TextureFormat::R8:
				return glm::vec4(pixel[0] / 255.0f, 0.0f, 0.0f, 1.0f);
			case TextureFormat::RGBA16F:
				return glm::vec4(HalfToFloat(ReadAt<uint16_t>(pixel, 0)), HalfToFloat(ReadAt<uint16_t>(pixel, 1)),
					HalfToFloat(ReadAt<uint16_t>(pixel, 2)), HalfToFloat(ReadAt<uint16_t>(pixel, 3)));
			case TextureFormat::R16F:
				return glm::vec4(HalfToFloat(ReadAt<uint16_t>(pixel, 0)), 0.0f, 0.0f, 1.0f);
			case TextureFormat::R32F:
			case TextureFormat::D32:
				return glm::vec4(ReadAt<float>(pixel, 0), 0.0f, 0.0f, 1.0f);
			case TextureFormat::RGBA32F:
				return glm::vec4(ReadAt<float>(pixel, 0), ReadAt<float>(pixel, 1), ReadAt<float>(pixel, 2), ReadAt<float>(pixel, 3));
			case TextureFormat::R11G11B10F:
			{
				const uint32_t packed = ReadAt<uint32_t>(pixel, 0);
				return glm::vec4(SmallFloatToFloat(packed & 0x7FFu, 6), SmallFloatToFloat((packed >> 11) & 0x7FFu, 6),
					SmallFloatToFloat((packed >> 22) & 0x3FFu, 5), 1.0f);
			}
			default:
				return glm::vec4(0.0f);
		}
	}

	Texture* Texture::CreateFromFile(const std::filesystem::path& filepath, const std::string& debugName)
	{
		const std::filesystem::path resolvedPath = Internal::ResolveRawAssetPath(filepath);

		uint32_t width = 0, height = 0, channels = 0;
		const uint8_t* data = FileSystem::ReadImage(resolvedPath, &width, &height, &channels, true, true);
		if (!data)
			return nullptr;

		// Upload() copies the pixels synchronously during Create(), so the CPU-side
		// buffer can (and must) be released here — it used to leak per load.
		Texture* texture = Create(TextureParams()
			.SetDebugName(debugName)
			.SetWidth(width)
			.SetHeight(height)
			.SetDimension(TextureDimension::Texture2D)
			.SetFormat(channels == 4 ? Dingo::TextureFormat::RGBA : Dingo::TextureFormat::RGB)
			.SetIsRenderTarget(false)
			.SetInitialData(data));
		FileSystem::FreeImage(data);

		return texture;
	}

	Texture* Texture::CreateFromData(uint32_t width, uint32_t height, const void* data, TextureFormat format, const std::string& debugName)
	{
		return Create(TextureParams()
			.SetDebugName(debugName)
			.SetWidth(width)
			.SetHeight(height)
			.SetDimension(TextureDimension::Texture2D)
			.SetFormat(format)
			.SetIsRenderTarget(false)
			.SetInitialData(data));
	}

	Texture* Texture::Create(const TextureParams& params)
	{
		Texture* texture = new NvrhiTexture(params);
		texture->Initialize();

		if (params.InitialData)
		{
			texture->Upload(params.InitialData, static_cast<uint64_t>(params.Width) * GetBytesPerPixel(params.Format));
		}

		return texture;
	}

	void Texture::SaveToFile(const std::filesystem::path& path, std::function<void(bool)> done)
	{
		const bool flip = !m_Params.IsRenderTarget;
		ReadPixels([path, flip, done = std::move(done)](const TexturePixels& pixels)
		{
			const bool rgba8 = pixels.Format == TextureFormat::RGBA8_UNORM || pixels.Format == TextureFormat::RGBA;
			const bool saved = rgba8 && !pixels.Data.empty() && FileSystem::WriteImage(path, pixels.Width, pixels.Height, 4, pixels.Data.data(), flip);
			if (done)
				done(saved);
		});
	}

	uint32_t Texture::NextGeneration()
	{
		static std::atomic<uint32_t> s_Next{ 1 };
		return s_Next.fetch_add(1, std::memory_order_relaxed);
	}

}
