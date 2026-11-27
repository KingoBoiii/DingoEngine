#include "TextureSubresourceTest.h"

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <format>
#include <fstream>

namespace Dingo
{

	namespace
	{
		constexpr const char* k_CubeWriteSource = R"(
#type compute
#version 450
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(push_constant) uniform PushData { uvec4 P; };
layout(binding = 0, rgba8) uniform writeonly image2DArray u_Cube;
void main()
{
	ivec3 size = imageSize(u_Cube);
	uvec3 id = gl_GlobalInvocationID;
	if (id.x >= uint(size.x) || id.y >= uint(size.y))
		return;
	vec4 value = vec4(float(id.z * 40u + P.x * 5u), float(id.x * 16u), float(id.y * 16u), 255.0) / 255.0;
	imageStore(u_Cube, ivec3(id), value);
}
)";

		constexpr const char* k_CubeSampleSource = R"(
#type vertex
#version 450
#include <DingoEngine/Fullscreen.glsl>

#type fragment
#version 450
#include <DingoEngine/CubeMap.glsl>
layout(location = 0) in vec2 v_TexCoord;
layout(push_constant) uniform PushData { vec4 P; };
layout(binding = 1) uniform textureCube u_Cube;
layout(binding = 2) uniform sampler u_Sampler;
layout(location = 0) out vec4 o_Color;
void main()
{
	ivec2 p = ivec2(gl_FragCoord.xy);
	int tile = int(P.y);
	uint face = uint(p.x / tile);
	vec2 uv = (vec2(float(p.x % tile), float(p.y)) + 0.5) / P.y;
	vec3 dir = DE_CubeFaceDirection(face, uv);
	o_Color = textureLod(samplerCube(u_Cube, u_Sampler), dir, P.x);
}
)";

		constexpr const char* k_ChainSource = R"(
#type compute
#version 450
#extension GL_EXT_samplerless_texture_functions : require
layout(local_size_x = 8, local_size_y = 8) in;
layout(binding = 0) uniform texture2D u_Source;
layout(binding = 1, rgba32f) uniform writeonly image2D u_Target;
void main()
{
	ivec2 p = ivec2(gl_GlobalInvocationID.xy);
	ivec2 size = imageSize(u_Target);
	if (p.x >= size.x || p.y >= size.y)
		return;
	ivec2 last = textureSize(u_Source, 0) - 1;
	ivec2 s = p * 2;
	vec4 sum = texelFetch(u_Source, min(s, last), 0)
		+ texelFetch(u_Source, min(s + ivec2(1, 0), last), 0)
		+ texelFetch(u_Source, min(s + ivec2(0, 1), last), 0)
		+ texelFetch(u_Source, min(s + ivec2(1, 1), last), 0);
	imageStore(u_Target, p, sum * 0.25);
}
)";

		constexpr const char* k_ChainProbeSource = R"(
#type compute
#version 450
#extension GL_EXT_samplerless_texture_functions : require
layout(local_size_x = 8, local_size_y = 8) in;
layout(binding = 0) uniform texture2D u_Chain;
layout(binding = 1, rgba32f) uniform writeonly image2D u_Probe;
void main()
{
	ivec2 p = ivec2(gl_GlobalInvocationID.xy);
	int level = p.y / 8;
	ivec2 q = ivec2(p.x, p.y % 8);
	vec4 value = vec4(0.0);
	if (level < 5)
	{
		ivec2 size = textureSize(u_Chain, level);
		if (q.x < size.x && q.y < size.y)
			value = texelFetch(u_Chain, q, level);
	}
	imageStore(u_Probe, p, value);
}
)";

		constexpr const char* k_VolumeWriteSource = R"(
#type compute
#version 450
layout(local_size_x = 4, local_size_y = 4, local_size_z = 4) in;
layout(binding = 0, rgba16f) uniform writeonly image3D u_Volume;
void main()
{
	ivec3 p = ivec3(gl_GlobalInvocationID);
	ivec3 size = imageSize(u_Volume);
	if (p.x >= size.x || p.y >= size.y || p.z >= size.z)
		return;
	imageStore(u_Volume, p, vec4(float(p.x), float(p.y), float(p.z), 1.0));
}
)";

		constexpr const char* k_VolumeSampleSource = R"(
#type vertex
#version 450
#include <DingoEngine/Fullscreen.glsl>

#type fragment
#version 450
#extension GL_EXT_samplerless_texture_functions : require
layout(location = 0) in vec2 v_TexCoord;
layout(push_constant) uniform PushData { vec4 P; };
layout(binding = 1) uniform texture3D u_Volume;
layout(binding = 2) uniform sampler u_Sampler;
layout(location = 0) out vec4 o_Color;
void main()
{
	ivec2 p = ivec2(gl_FragCoord.xy);
	int z = p.x / 16;
	int x = p.x % 16;
	if (P.x < 0.5)
		o_Color = texelFetch(u_Volume, ivec3(x, p.y, z), 0);
	else
		o_Color = texture(sampler3D(u_Volume, u_Sampler), vec3((float(x) + 0.5) / 16.0, (float(p.y) + 0.5) / 16.0, float(z + 1) / 8.0));
}
)";

		constexpr const char* k_BindFillSource = R"(
#type compute
#version 450
layout(local_size_x = 8, local_size_y = 8) in;
layout(push_constant) uniform PushData { vec4 Color; };
layout(binding = 0, rgba8) uniform writeonly image2D u_Image;
void main()
{
	imageStore(u_Image, ivec2(gl_GlobalInvocationID.xy), Color);
}
)";

		constexpr const char* k_BindSampleSource = R"(
#type vertex
#version 450
#include <DingoEngine/Fullscreen.glsl>

#type fragment
#version 450
layout(location = 0) in vec2 v_TexCoord;
layout(binding = 1) uniform texture2D u_Source;
layout(binding = 2) uniform sampler u_Sampler;
layout(location = 0) out vec4 o_Color;
void main() { o_Color = texture(sampler2D(u_Source, u_Sampler), v_TexCoord); }
)";

		Material* MakeMaterial(const char* name, Shader* shader, Texture* texture, Sampler* sampler)
		{
			Material* material = Material::Create(MaterialParams()
				.SetDebugName(name)
				.SetShader(shader)
				.SetCullMode(CullMode::None)
				.SetDepthTest(false)
				.SetDepthWrite(false)
				.SetBlendMode(BlendMode::Opaque));
			material->SetTexture(0, texture);
			material->SetSampler(0, sampler);
			return material;
		}

		Framebuffer* MakeTarget(const char* name, uint32_t width, uint32_t height, TextureFormat format)
		{
			return Framebuffer::Create(FramebufferParams()
				.SetDebugName(name)
				.SetWidth(static_cast<int32_t>(width))
				.SetHeight(static_cast<int32_t>(height))
				.AddAttachment({ format }));
		}

		Texture* MakeCube(const char* name, TextureFormat format, bool storage)
		{
			return Texture::Create(TextureParams()
				.SetDebugName(name)
				.SetWidth(16)
				.SetHeight(16)
				.SetMipLevels(5)
				.SetFormat(format)
				.SetDimension(TextureDimension::TextureCube)
				.SetIsStorage(storage));
		}

		std::array<uint8_t, 4> Pattern8(uint32_t mip, uint32_t face, uint32_t x, uint32_t y)
		{
			return { static_cast<uint8_t>(face * 40 + mip * 5), static_cast<uint8_t>(x * 16), static_cast<uint8_t>(y * 16), 255 };
		}

		glm::vec4 PatternHalf(uint32_t mip, uint32_t face, uint32_t x, uint32_t y)
		{
			return glm::vec4(static_cast<float>(face + mip * 8), static_cast<float>(x), static_cast<float>(y), 1.0f);
		}

		// Exact for the small whole numbers the half pattern holds.
		uint16_t HalfBits(uint32_t value)
		{
			if (value == 0)
				return 0;
			const uint32_t exponent = static_cast<uint32_t>(std::bit_width(value)) - 1;
			const uint32_t mantissa = (value - (1u << exponent)) << (10 - exponent);
			return static_cast<uint16_t>(((exponent + 15) << 10) | mantissa);
		}

		bool Near(const glm::vec4& a, const glm::vec4& b, float tolerance)
		{
			for (int i = 0; i < 4; ++i)
			{
				if (std::abs(a[i] - b[i]) > tolerance * (std::max)(1.0f, std::abs(b[i])))
					return false;
			}
			return true;
		}

		glm::vec4 ChainSource(uint32_t x, uint32_t y)
		{
			return glm::vec4(static_cast<float>(x + 32 * y), static_cast<float>(3 * y + 1), static_cast<float>(x * x), 2.0f);
		}

		struct Tally
		{
			uint32_t Remaining = 0;
			uint32_t Wrong = 0;
		};

		// 4 x 2 flat RGBE pixels, file rows top first: r, g, b are mantissas, e the shared exponent.
		constexpr uint8_t k_HdrPixels[8][4] = {
			{ 128, 64, 32, 129 }, { 255, 0, 0, 136 }, { 0, 0, 0, 0 }, { 200, 100, 50, 145 },
			{ 1, 2, 3, 136 }, { 128, 128, 128, 128 }, { 100, 200, 50, 130 }, { 255, 255, 255, 136 },
		};
		// The decoded file as the texture holds it: the loader flips rows, so row 0 is the file's last.
		const glm::vec4 k_HdrExpected[2][4] = {
			{ glm::vec4(1.0f, 2.0f, 3.0f, 1.0f), glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), glm::vec4(1.5625f, 3.125f, 0.78125f, 1.0f), glm::vec4(255.0f, 255.0f, 255.0f, 1.0f) },
			{ glm::vec4(1.0f, 0.5f, 0.25f, 1.0f), glm::vec4(255.0f, 0.0f, 0.0f, 1.0f), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), glm::vec4(102400.0f, 51200.0f, 25600.0f, 1.0f) },
		};
	}

	void TextureSubresourceTest::Initialize()
	{
		m_Checks.clear();
		m_ChecksDone = false;
		m_Alive = std::make_shared<int>(0);

		m_UploadedCube = MakeCube("SubresourceTest uploaded cube", TextureFormat::RGBA8_UNORM, false);
		m_HalfCube = MakeCube("SubresourceTest half cube", TextureFormat::RGBA16F, false);
		m_ComputeCube = MakeCube("SubresourceTest compute cube", TextureFormat::RGBA8_UNORM, true);
		if (m_UploadedCube && m_HalfCube)
		{
			for (uint32_t mip = 0; mip < k_CubeMips; ++mip)
			{
				const uint32_t size = k_CubeSize >> mip;
				for (uint32_t face = 0; face < 6; ++face)
				{
					std::vector<uint8_t> bytes(static_cast<size_t>(size) * size * 4);
					std::vector<uint16_t> halves(static_cast<size_t>(size) * size * 4);
					for (uint32_t y = 0; y < size; ++y)
					{
						for (uint32_t x = 0; x < size; ++x)
						{
							const size_t offset = (static_cast<size_t>(y) * size + x) * 4;
							const std::array<uint8_t, 4> value = Pattern8(mip, face, x, y);
							const glm::vec4 half = PatternHalf(mip, face, x, y);
							for (size_t c = 0; c < 4; ++c)
							{
								bytes[offset + c] = value[c];
								halves[offset + c] = HalfBits(static_cast<uint32_t>(half[static_cast<int>(c)]));
							}
						}
					}
					m_UploadedCube->UploadSubresource(bytes.data(), mip, face);
					m_HalfCube->UploadSubresource(halves.data(), mip, face);
				}
			}
		}

		std::vector<glm::vec4> chainData(static_cast<size_t>(k_ChainWidth) * k_ChainHeight);
		for (uint32_t y = 0; y < k_ChainHeight; ++y)
		{
			for (uint32_t x = 0; x < k_ChainWidth; ++x)
				chainData[static_cast<size_t>(y) * k_ChainWidth + x] = ChainSource(x, y);
		}
		m_Chain = Texture::Create(TextureParams()
			.SetDebugName("SubresourceTest chain")
			.SetWidth(k_ChainWidth)
			.SetHeight(k_ChainHeight)
			.SetMipLevels(0)
			.SetFormat(TextureFormat::RGBA32F)
			.SetDimension(TextureDimension::Texture2D)
			.SetIsStorage(true)
			.SetInitialData(chainData.data()));

		m_ChainProbe = Texture::Create(TextureParams()
			.SetDebugName("SubresourceTest chain probe")
			.SetWidth(k_ProbeWidth)
			.SetHeight(k_ProbeRows * k_ProbeMips)
			.SetFormat(TextureFormat::RGBA32F)
			.SetDimension(TextureDimension::Texture2D)
			.SetIsStorage(true));

		m_Volume = Texture::Create(TextureParams()
			.SetDebugName("SubresourceTest volume")
			.SetWidth(k_VolumeSize)
			.SetHeight(k_VolumeSize)
			.SetDepth(k_VolumeDepth)
			.SetFormat(TextureFormat::RGBA16F)
			.SetDimension(TextureDimension::Texture3D)
			.SetIsStorage(true));

		std::vector<uint8_t> redPixels(static_cast<size_t>(k_BindSize) * k_BindSize * 4);
		for (size_t i = 0; i < redPixels.size(); i += 4)
		{
			redPixels[i] = 255;
			redPixels[i + 3] = 255;
		}
		m_Bound = Texture::Create(TextureParams()
			.SetDebugName("SubresourceTest bound")
			.SetWidth(k_BindSize)
			.SetHeight(k_BindSize)
			.SetFormat(TextureFormat::RGBA8_UNORM)
			.SetDimension(TextureDimension::Texture2D)
			.SetIsStorage(true)
			.SetInitialData(redPixels.data()));

		m_CubeWriteShader = Shader::CreateFromSource("SubresourceTestCubeWrite", k_CubeWriteSource);
		m_CubeSampleShader = Shader::CreateFromSource("SubresourceTestCubeSample", k_CubeSampleSource);
		m_ChainShader = Shader::CreateFromSource("SubresourceTestChain", k_ChainSource);
		m_ChainProbeShader = Shader::CreateFromSource("SubresourceTestChainProbe", k_ChainProbeSource);
		m_VolumeWriteShader = Shader::CreateFromSource("SubresourceTestVolumeWrite", k_VolumeWriteSource);
		m_VolumeSampleShader = Shader::CreateFromSource("SubresourceTestVolumeSample", k_VolumeSampleSource);
		m_BindFillShader = Shader::CreateFromSource("SubresourceTestBindFill", k_BindFillSource);
		m_BindSampleShader = Shader::CreateFromSource("SubresourceTestBindSample", k_BindSampleSource);

		for (uint32_t mip = 0; mip < k_CubeMips; ++mip)
		{
			m_CubeWritePasses[mip] = ComputePass::Create(ComputePassParams().SetDebugName("SubresourceTest cube write").SetShader(m_CubeWriteShader));
			m_CubeWritePasses[mip]->SetStorageTexture(0, m_ComputeCube, mip);
		}
		for (uint32_t i = 0; i + 1 < k_ChainMips; ++i)
		{
			m_ChainPasses[i] = ComputePass::Create(ComputePassParams().SetDebugName("SubresourceTest chain").SetShader(m_ChainShader));
			m_ChainPasses[i]->SetTexture(0, m_Chain, i, 1);
			m_ChainPasses[i]->SetStorageTexture(1, m_Chain, i + 1);
		}
		m_ChainProbePass = ComputePass::Create(ComputePassParams().SetDebugName("SubresourceTest chain probe").SetShader(m_ChainProbeShader));
		m_ChainProbePass->SetTexture(0, m_Chain, 1);
		m_ChainProbePass->SetStorageTexture(1, m_ChainProbe);
		m_VolumeWritePass = ComputePass::Create(ComputePassParams().SetDebugName("SubresourceTest volume write").SetShader(m_VolumeWriteShader));
		m_VolumeWritePass->SetStorageTexture(0, m_Volume);
		m_BindFillPass = ComputePass::Create(ComputePassParams().SetDebugName("SubresourceTest bind fill").SetShader(m_BindFillShader));
		m_BindFillPass->SetStorageTexture(0, m_Bound);

		m_UploadedCubeMaterial = MakeMaterial("SubresourceTest uploaded cube", m_CubeSampleShader, m_UploadedCube, Renderer::GetPointSampler());
		m_ComputeCubeMaterial = MakeMaterial("SubresourceTest compute cube", m_CubeSampleShader, m_ComputeCube, Renderer::GetPointSampler());
		m_VolumeMaterial = MakeMaterial("SubresourceTest volume", m_VolumeSampleShader, m_Volume, Renderer::GetClampSampler());
		m_BindMaterial = MakeMaterial("SubresourceTest bind", m_BindSampleShader, m_Bound, Renderer::GetPointSampler());

		m_UploadedMip0 = MakeTarget("SubresourceTest uploaded mip 0", 6 * 16, 16, TextureFormat::RGBA8_UNORM);
		m_UploadedMip2 = MakeTarget("SubresourceTest uploaded mip 2", 6 * 4, 4, TextureFormat::RGBA8_UNORM);
		m_ComputeMip0 = MakeTarget("SubresourceTest compute mip 0", 6 * 16, 16, TextureFormat::RGBA8_UNORM);
		m_ComputeMip2 = MakeTarget("SubresourceTest compute mip 2", 6 * 4, 4, TextureFormat::RGBA8_UNORM);
		m_VolumeFetch = MakeTarget("SubresourceTest volume fetch", k_VolumeSize * k_VolumeDepth, k_VolumeSize, TextureFormat::RGBA32F);
		m_VolumeLinear = MakeTarget("SubresourceTest volume linear", k_VolumeSize * k_VolumeDepth, k_VolumeSize, TextureFormat::RGBA32F);
		for (Framebuffer*& target : m_BindTargets)
			target = MakeTarget("SubresourceTest bound target", k_BindSize, k_BindSize, TextureFormat::RGBA8_UNORM);

		m_HdrFolder = TestTempDirectory("DingoSubresourceTest");
		std::error_code error;
		std::filesystem::create_directories(m_HdrFolder, error);
		const std::filesystem::path file = m_HdrFolder / "flat.hdr";
		{
			std::ofstream out(file, std::ios::binary);
			const char header[] = "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 2 +X 4\n";
			out.write(header, sizeof(header) - 1);
			out.write(reinterpret_cast<const char*>(k_HdrPixels), sizeof(k_HdrPixels));
		}
		m_HdrHalf = Texture::CreateFromHdrFile(file, TextureFormat::RGBA16F, "SubresourceTest hdr half");
		m_HdrFloat = Texture::CreateFromHdrFile(file, TextureFormat::RGBA32F, "SubresourceTest hdr float");
	}

	void TextureSubresourceTest::ReadBack(Texture* texture, std::function<void(const TexturePixels&)> check)
	{
		const std::weak_ptr<int> alive = m_Alive;
		texture->ReadPixels([alive, check = std::move(check)](const TexturePixels& pixels)
		{
			if (!alive.expired())
				check(pixels);
		});
	}

	void TextureSubresourceTest::ReadSubresources(Texture* texture, uint32_t mips, uint32_t layers, SubresourceCheck verify, const std::string& name)
	{
		const std::weak_ptr<int> alive = m_Alive;
		const uint32_t total = mips * layers;
		auto tally = std::make_shared<Tally>();
		tally->Remaining = total;
		auto check = std::make_shared<SubresourceCheck>(std::move(verify));
		for (uint32_t mip = 0; mip < mips; ++mip)
		{
			for (uint32_t layer = 0; layer < layers; ++layer)
			{
				texture->ReadSubresource(mip, layer, [this, alive, tally, check, mip, layer, total, name](const TexturePixels& pixels)
				{
					if (alive.expired())
						return;
					if (pixels.Data.empty() || !(*check)(mip, layer, pixels))
						++tally->Wrong;
					if (--tally->Remaining == 0)
						Check(tally->Wrong == 0, std::format("{} ({} of {} subresources wrong)", name, tally->Wrong, total));
				});
			}
		}
	}

	void TextureSubresourceTest::DrawTiles(Material* material, Framebuffer* target, const float (&push)[4])
	{
		Framebuffer* previous = Renderer::GetRenderTarget();
		Renderer::SetRenderTarget(target);
		Renderer::Clear(target, glm::vec4(0.0f));
		material->SetPushConstants(push, sizeof(push));
		Renderer::Draw(material, 3);
		Renderer::SetRenderTarget(previous);
	}

	void TextureSubresourceTest::CheckCubeRoundTrip()
	{
		if (!m_UploadedCube || !m_HalfCube)
		{
			Check(false, "the cube textures were created");
			return;
		}

		ReadSubresources(m_UploadedCube, k_CubeMips, 6, [](uint32_t mip, uint32_t face, const TexturePixels& pixels)
		{
			const uint32_t size = k_CubeSize >> mip;
			if (pixels.Width != size || pixels.Height != size || pixels.Format != TextureFormat::RGBA8_UNORM)
				return false;
			for (uint32_t y = 0; y < size; ++y)
			{
				for (uint32_t x = 0; x < size; ++x)
				{
					const std::array<uint8_t, 4> want = Pattern8(mip, face, x, y);
					if (std::memcmp(pixels.Data.data() + (static_cast<size_t>(y) * size + x) * 4, want.data(), 4) != 0)
						return false;
				}
			}
			return true;
		}, "a 16 x 16 RGBA8 cube with 5 mips returns every uploaded subresource exactly");

		ReadSubresources(m_HalfCube, k_CubeMips, 6, [](uint32_t mip, uint32_t face, const TexturePixels& pixels)
		{
			const uint32_t size = k_CubeSize >> mip;
			if (pixels.Width != size || pixels.Height != size || pixels.Format != TextureFormat::RGBA16F)
				return false;
			for (uint32_t y = 0; y < size; ++y)
			{
				for (uint32_t x = 0; x < size; ++x)
				{
					if (pixels.GetPixel(x, y) != PatternHalf(mip, face, x, y))
						return false;
				}
			}
			return true;
		}, "a 16 x 16 RGBA16F cube with 5 mips returns every uploaded subresource exactly");
	}

	void TextureSubresourceTest::CheckCubeSampling()
	{
		struct Case
		{
			Material* Mat;
			Framebuffer* Target;
			uint32_t Lod;
			const char* Name;
		};
		const Case cases[] = {
			{ m_UploadedCubeMaterial, m_UploadedMip0, 0, "a pixel shader sampling an uploaded cube by face direction finds every texel at mip 0" },
			{ m_UploadedCubeMaterial, m_UploadedMip2, 2, "a pixel shader sampling an uploaded cube by face direction finds every texel at mip 2" },
			{ m_ComputeCubeMaterial, m_ComputeMip0, 0, "a pixel shader sees a cube a compute shader wrote, at mip 0" },
			{ m_ComputeCubeMaterial, m_ComputeMip2, 2, "a pixel shader sees a cube a compute shader wrote, at mip 2" },
		};
		for (const Case& c : cases)
		{
			const uint32_t tile = k_CubeSize >> c.Lod;
			const float push[4] = { static_cast<float>(c.Lod), static_cast<float>(tile), 0.0f, 0.0f };
			DrawTiles(c.Mat, c.Target, push);
			ReadBack(c.Target->GetAttachment(0), [this, lod = c.Lod, tile, name = std::string(c.Name)](const TexturePixels& pixels)
			{
				if (pixels.Data.empty())
				{
					Check(false, name + " (no readback)");
					return;
				}
				uint32_t wrong = 0;
				for (uint32_t face = 0; face < 6; ++face)
				{
					for (uint32_t y = 0; y < tile; ++y)
					{
						for (uint32_t x = 0; x < tile; ++x)
						{
							const std::array<uint8_t, 4> want = Pattern8(lod, face, x, y);
							wrong += std::memcmp(pixels.Data.data() + (static_cast<size_t>(y) * pixels.Width + face * tile + x) * 4, want.data(), 4) != 0 ? 1 : 0;
						}
					}
				}
				Check(wrong == 0, std::format("{} ({} of {} texels wrong)", name, wrong, 6 * tile * tile));
			});
		}
	}

	void TextureSubresourceTest::CheckCubeCompute()
	{
		for (uint32_t mip = 0; mip < k_CubeMips; ++mip)
		{
			const uint32_t size = k_CubeSize >> mip;
			const uint32_t push[4] = { mip, 0, 0, 0 };
			m_CubeWritePasses[mip]->SetPushConstants(push, sizeof(push));
			Renderer::Dispatch(m_CubeWritePasses[mip], (size + 7) / 8, (size + 7) / 8, 6);
		}
		ReadSubresources(m_ComputeCube, k_CubeMips, 6, [](uint32_t mip, uint32_t face, const TexturePixels& pixels)
		{
			const uint32_t size = k_CubeSize >> mip;
			if (pixels.Width != size || pixels.Height != size)
				return false;
			for (uint32_t y = 0; y < size; ++y)
			{
				for (uint32_t x = 0; x < size; ++x)
				{
					const std::array<uint8_t, 4> want = Pattern8(mip, face, x, y);
					if (std::memcmp(pixels.Data.data() + (static_cast<size_t>(y) * size + x) * 4, want.data(), 4) != 0)
						return false;
				}
			}
			return true;
		}, "a compute shader writes every mip and face of a storage cube through image2DArray");
	}

	void TextureSubresourceTest::CheckMipChain()
	{
		if (!m_Chain)
		{
			Check(false, "the mip chain texture was created");
			return;
		}
		for (uint32_t i = 0; i + 1 < k_ChainMips; ++i)
		{
			const uint32_t width = (std::max)(k_ChainWidth >> (i + 1), 1u);
			const uint32_t height = (std::max)(k_ChainHeight >> (i + 1), 1u);
			Renderer::Dispatch(m_ChainPasses[i], (width + 7) / 8, (height + 7) / 8);
		}
		Renderer::Dispatch(m_ChainProbePass, k_ProbeWidth / 8, k_ProbeRows * k_ProbeMips / 8);

		std::vector<std::vector<glm::vec4>> chain(k_ChainMips);
		chain[0].resize(static_cast<size_t>(k_ChainWidth) * k_ChainHeight);
		for (uint32_t y = 0; y < k_ChainHeight; ++y)
		{
			for (uint32_t x = 0; x < k_ChainWidth; ++x)
				chain[0][static_cast<size_t>(y) * k_ChainWidth + x] = ChainSource(x, y);
		}
		for (uint32_t mip = 1; mip < k_ChainMips; ++mip)
		{
			const uint32_t sw = (std::max)(k_ChainWidth >> (mip - 1), 1u);
			const uint32_t sh = (std::max)(k_ChainHeight >> (mip - 1), 1u);
			const uint32_t dw = (std::max)(k_ChainWidth >> mip, 1u);
			const uint32_t dh = (std::max)(k_ChainHeight >> mip, 1u);
			chain[mip].resize(static_cast<size_t>(dw) * dh);
			for (uint32_t y = 0; y < dh; ++y)
			{
				for (uint32_t x = 0; x < dw; ++x)
				{
					const auto at = [&](uint32_t sx, uint32_t sy) { return chain[mip - 1][static_cast<size_t>((std::min)(sy, sh - 1)) * sw + (std::min)(sx, sw - 1)]; };
					chain[mip][static_cast<size_t>(y) * dw + x] = (at(2 * x, 2 * y) + at(2 * x + 1, 2 * y) + at(2 * x, 2 * y + 1) + at(2 * x + 1, 2 * y + 1)) * 0.25f;
				}
			}
		}

		ReadSubresources(m_ChainProbe, 1, 1, [chain](uint32_t, uint32_t, const TexturePixels& pixels)
		{
			if (pixels.Width != k_ProbeWidth || pixels.Height != k_ProbeRows * k_ProbeMips)
				return false;
			for (uint32_t level = 0; level < k_ProbeMips; ++level)
			{
				const uint32_t width = (std::max)(k_ChainWidth >> (level + 1), 1u);
				const uint32_t height = (std::max)(k_ChainHeight >> (level + 1), 1u);
				for (uint32_t y = 0; y < k_ProbeRows; ++y)
				{
					for (uint32_t x = 0; x < k_ProbeWidth; ++x)
					{
						const bool inside = x < width && y < height;
						const glm::vec4 want = inside ? chain[level + 1][static_cast<size_t>(y) * width + x] : glm::vec4(0.0f);
						if (pixels.GetPixel(x, level * k_ProbeRows + y) != want)
							return false;
					}
				}
			}
			return true;
		}, "a pass binding the chain from mip 1 with the default mip count reads mips 1 to 5 as its own levels 0 to 4, after they were written");

		ReadSubresources(m_Chain, k_ChainMips, 1, [chain = std::move(chain)](uint32_t mip, uint32_t, const TexturePixels& pixels)
		{
			const uint32_t width = (std::max)(k_ChainWidth >> mip, 1u);
			const uint32_t height = (std::max)(k_ChainHeight >> mip, 1u);
			if (pixels.Width != width || pixels.Height != height)
				return false;
			for (uint32_t y = 0; y < height; ++y)
			{
				for (uint32_t x = 0; x < width; ++x)
				{
					if (pixels.GetPixel(x, y) != chain[mip][static_cast<size_t>(y) * width + x])
						return false;
				}
			}
			return true;
		}, "a 32 x 16 RGBA32F chain built by passes reading mip N - 1 and writing mip N holds the exact 2 x 2 averages");
	}

	void TextureSubresourceTest::CheckVolume()
	{
		if (!m_Volume)
		{
			Check(false, "the 3D texture was created");
			return;
		}
		Renderer::Dispatch(m_VolumeWritePass, k_VolumeSize / 4, k_VolumeSize / 4, k_VolumeDepth / 4);

		ReadSubresources(m_Volume, 1, k_VolumeDepth, [](uint32_t, uint32_t slice, const TexturePixels& pixels)
		{
			if (pixels.Width != k_VolumeSize || pixels.Height != k_VolumeSize)
				return false;
			for (uint32_t y = 0; y < k_VolumeSize; ++y)
			{
				for (uint32_t x = 0; x < k_VolumeSize; ++x)
				{
					if (pixels.GetPixel(x, y) != glm::vec4(static_cast<float>(x), static_cast<float>(y), static_cast<float>(slice), 1.0f))
						return false;
				}
			}
			return true;
		}, "a compute shader writes a 16 x 16 x 8 RGBA16F texture through image3D, read back slice by slice");

		const float fetch[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		const float linear[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
		DrawTiles(m_VolumeMaterial, m_VolumeFetch, fetch);
		DrawTiles(m_VolumeMaterial, m_VolumeLinear, linear);

		ReadBack(m_VolumeFetch->GetAttachment(0), [this](const TexturePixels& pixels)
		{
			if (pixels.Data.empty())
			{
				Check(false, "texelFetch from a 3D texture (no readback)");
				return;
			}
			uint32_t wrong = 0;
			for (uint32_t z = 0; z < k_VolumeDepth; ++z)
			{
				for (uint32_t y = 0; y < k_VolumeSize; ++y)
				{
					for (uint32_t x = 0; x < k_VolumeSize; ++x)
						wrong += pixels.GetPixel(z * k_VolumeSize + x, y) != glm::vec4(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 1.0f) ? 1 : 0;
				}
			}
			Check(wrong == 0, std::format("a pixel shader texelFetches every voxel of the 3D texture ({} of {} wrong)", wrong, k_VolumeSize * k_VolumeSize * k_VolumeDepth));
		});

		ReadBack(m_VolumeLinear->GetAttachment(0), [this](const TexturePixels& pixels)
		{
			if (pixels.Data.empty())
			{
				Check(false, "linear sampling of a 3D texture (no readback)");
				return;
			}
			uint32_t wrong = 0;
			for (uint32_t z = 0; z + 1 < k_VolumeDepth; ++z)
			{
				for (uint32_t y = 0; y < k_VolumeSize; ++y)
				{
					for (uint32_t x = 0; x < k_VolumeSize; ++x)
						wrong += !Near(pixels.GetPixel(z * k_VolumeSize + x, y), glm::vec4(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z) + 0.5f, 1.0f), 0.01f) ? 1 : 0;
				}
			}
			Check(wrong == 0, std::format("linear sampling halfway between two slices blends them ({} of {} wrong)", wrong, k_VolumeSize * k_VolumeSize * (k_VolumeDepth - 1)));
		});
	}

	void TextureSubresourceTest::CheckRebinding()
	{
		const glm::vec4 colors[2] = { glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f) };
		Framebuffer* previous = Renderer::GetRenderTarget();
		for (uint32_t i = 0; i < 3; ++i)
		{
			Renderer::SetRenderTarget(m_BindTargets[i]);
			Renderer::Clear(m_BindTargets[i], glm::vec4(0.0f));
			Renderer::Draw(m_BindMaterial, 3);
			if (i < 2)
			{
				m_BindFillPass->SetPushConstants(&colors[i], sizeof(glm::vec4));
				Renderer::Dispatch(m_BindFillPass, 1, 1);
			}
		}
		Renderer::SetRenderTarget(previous);

		const std::array<std::array<uint8_t, 4>, 3> want = { { { 255, 0, 0, 255 }, { 0, 255, 0, 255 }, { 0, 0, 255, 255 } } };
		for (uint32_t i = 0; i < 3; ++i)
		{
			ReadBack(m_BindTargets[i]->GetAttachment(0), [this, i, w = want[i]](const TexturePixels& pixels)
			{
				bool match = !pixels.Data.empty();
				for (uint32_t p = 0; match && p < k_BindSize * k_BindSize; ++p)
					match = std::memcmp(pixels.Data.data() + static_cast<size_t>(p) * 4, w.data(), 4) == 0;
				Check(match, std::format("one binding set samples what a compute pass wrote before draw {} of 3", i + 1));
			});
		}
	}

	void TextureSubresourceTest::CheckHdr()
	{
		struct Case
		{
			Texture* Image;
			bool Half;
			const char* Name;
		};
		const Case cases[] = {
			{ m_HdrHalf, true, "a .hdr loads as RGBA16F, flipped, alpha 1, values above 65504 clamped" },
			{ m_HdrFloat, false, "a .hdr loads as RGBA32F, flipped, alpha 1, values exact" },
		};
		for (const Case& c : cases)
		{
			if (!c.Image)
			{
				Check(false, std::string(c.Name) + " (the load failed)");
				continue;
			}
			ReadBack(c.Image, [this, half = c.Half, format = c.Half ? TextureFormat::RGBA16F : TextureFormat::RGBA32F, name = std::string(c.Name)](const TexturePixels& pixels)
			{
				if (pixels.Data.empty() || pixels.Width != 4 || pixels.Height != 2 || pixels.Format != format)
				{
					Check(false, name + " (wrong size, format or no readback)");
					return;
				}
				uint32_t wrong = 0;
				for (uint32_t y = 0; y < 2; ++y)
				{
					for (uint32_t x = 0; x < 4; ++x)
					{
						glm::vec4 expected = k_HdrExpected[y][x];
						if (half)
							expected = glm::min(expected, glm::vec4(65504.0f));
						wrong += Near(pixels.GetPixel(x, y), expected, half ? 1e-3f : 0.0f) ? 0 : 1;
					}
				}
				Check(wrong == 0, std::format("{} ({} of 8 texels wrong)", name, wrong));
			});
		}
	}

	void TextureSubresourceTest::RunChecks()
	{
		CheckCubeRoundTrip();
		CheckCubeCompute();
		CheckCubeSampling();
		CheckMipChain();
		CheckVolume();
		CheckRebinding();
		CheckHdr();
	}

	void TextureSubresourceTest::Update(float deltaTime)
	{
		if (!m_ChecksDone && !Renderer::IsFrameSkipped())
		{
			m_ChecksDone = true;
			RunChecks();
		}

		Renderer2D& renderer2D = Application::Get().GetRenderer2D();
		renderer2D.BeginScene(glm::ortho(-m_AspectRatio, m_AspectRatio, -1.0f, 1.0f, -1.0f, 1.0f));
		renderer2D.Clear(m_ClearColor);
		renderer2D.DrawQuad({ 0.0f, 0.3f }, { 1.8f, 0.3f }, m_UploadedMip0->GetAttachment(0));
		renderer2D.DrawQuad({ 0.0f, -0.3f }, { 1.8f, 0.3f }, m_ComputeMip0->GetAttachment(0));
		renderer2D.EndScene();
	}

	void TextureSubresourceTest::Cleanup()
	{
		m_Alive.reset();
		for (ComputePass*& pass : m_CubeWritePasses)
			DestroyAndDelete(pass);
		for (ComputePass*& pass : m_ChainPasses)
			DestroyAndDelete(pass);
		DestroyAndDelete(m_ChainProbePass);
		DestroyAndDelete(m_VolumeWritePass);
		DestroyAndDelete(m_BindFillPass);
		DestroyAndDelete(m_UploadedCubeMaterial);
		DestroyAndDelete(m_ComputeCubeMaterial);
		DestroyAndDelete(m_VolumeMaterial);
		DestroyAndDelete(m_BindMaterial);
		DestroyAndDelete(m_CubeWriteShader);
		DestroyAndDelete(m_CubeSampleShader);
		DestroyAndDelete(m_ChainShader);
		DestroyAndDelete(m_ChainProbeShader);
		DestroyAndDelete(m_VolumeWriteShader);
		DestroyAndDelete(m_VolumeSampleShader);
		DestroyAndDelete(m_BindFillShader);
		DestroyAndDelete(m_BindSampleShader);
		DestroyAndDelete(m_UploadedMip0);
		DestroyAndDelete(m_UploadedMip2);
		DestroyAndDelete(m_ComputeMip0);
		DestroyAndDelete(m_ComputeMip2);
		DestroyAndDelete(m_VolumeFetch);
		DestroyAndDelete(m_VolumeLinear);
		for (Framebuffer*& target : m_BindTargets)
			DestroyAndDelete(target);
		DestroyAndDelete(m_UploadedCube);
		DestroyAndDelete(m_HalfCube);
		DestroyAndDelete(m_ComputeCube);
		DestroyAndDelete(m_Chain);
		DestroyAndDelete(m_ChainProbe);
		DestroyAndDelete(m_Volume);
		DestroyAndDelete(m_Bound);
		DestroyAndDelete(m_HdrHalf);
		DestroyAndDelete(m_HdrFloat);
		std::error_code error;
		std::filesystem::remove_all(m_HdrFolder, error);
		m_Checks.clear();
	}

	void TextureSubresourceTest::Resize(uint32_t width, uint32_t height)
	{
		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
	}

	void TextureSubresourceTest::ImGuiRender()
	{
		ImGui::TextWrapped("Top: the uploaded cube sampled by face direction. Bottom: the cube a compute shader wrote, sampled the same way.");
		GraphicsTest::ImGuiRender();

		ImGui::Separator();
		for (const CheckResult& check : m_Checks)
		{
			const ImVec4 color = check.Passed ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
			ImGui::TextColored(color, "%s %s", check.Passed ? "[PASS]" : "[FAIL]", check.Name.c_str());
		}
	}

}
