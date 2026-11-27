#include "PbrMaterialTest.h"

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <limits>

// The engine's own Brdf.glsl compiled as C++ over GLM (as BrdfTests.cpp does), so the CPU evaluation
// checked against the GPU is the shader's text. The macros would swallow the calls to max.
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
namespace GlslBrdf
{
	using glm::vec2;
	using glm::vec3;
	using glm::vec4;
	using glm::clamp;
	using glm::exp2;
	using glm::max;
	using glm::min;
	using glm::mix;
	using glm::pow;
	using glm::sqrt;
	using glm::step;

#include "DingoEngine/Graphics/Shaders/Brdf.glsl"
}
#pragma pop_macro("max")
#pragma pop_macro("min")

namespace
{
	constexpr uint32_t k_CellPixels = 63;
	constexpr uint32_t k_GridCells = 5;
	constexpr float k_SphereRadius = 0.4f;
	constexpr float k_Tolerance = 2.0f / 255.0f + 1e-4f;
	constexpr float k_InstanceTolerance = 2.5f / 255.0f;

	constexpr float k_Metallic[k_GridCells] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
	constexpr float k_Roughness[k_GridCells] = { 0.1f, 0.3f, 0.5f, 0.75f, 1.0f };
	constexpr float k_LightIntensity = 0.3f;
	// The light colour is sRGB, so this decodes to about 1.5e-4 linear: the roughness-0.1 column's
	// peak (D * V near 800 for a metal) then stays below 1 instead of clipping at the centre texel.
	constexpr float k_PeakLightIntensity = 0.002f;
		constexpr float k_ViewHalfHeight = 4.6f;
		constexpr float k_GridScale = 0.85f;
		constexpr float k_GridCentreX = -1.5f;
		constexpr float k_PanelCentreX = 3.1f;
		constexpr float k_PanelSpacing = 1.3f;
		constexpr float k_PanelScale = 1.4f;

	const glm::vec4 k_Black{ 0.0f, 0.0f, 0.0f, 1.0f };
	const glm::vec4 k_Backdrop{ 0.2f, 0.4f, 0.6f, 1.0f };

	// An odd cell size puts a sphere's centre on a texel centre, and rings and segments that are
	// multiples of 4 put a mesh vertex at (0, 0, +r): the normal there is exactly +z.
	constexpr uint32_t k_ExactRings = 48;
	constexpr uint32_t k_ExactSegments = 48;

	glm::mat4 ViewProjection(uint32_t cols, uint32_t rows)
	{
		const float halfWidth = 0.5f * static_cast<float>(cols);
		const float halfHeight = 0.5f * static_cast<float>(rows);
		return glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, -5.0f, 5.0f);
	}

	// The centre of cell (column, row), row 0 on top.
	glm::mat4 CellAt(uint32_t cols, uint32_t rows, uint32_t column, uint32_t row)
	{
		const float x = static_cast<float>(column) + 0.5f - 0.5f * static_cast<float>(cols);
		const float y = 0.5f * static_cast<float>(rows) - 0.5f - static_cast<float>(row);
		return glm::translate(glm::mat4(1.0f), { x, y, 0.0f });
	}

	// The camera's light alone, travelling along -z, plus an optional flat ambient.
	void LightFromCamera(Dingo::Renderer3D& renderer, const glm::vec3& color, float intensity, const glm::vec3& ambient)
	{
		renderer.SetAmbientLight(ambient, 1.0f);
		Dingo::DirectionalLight light;
		light.Direction = { 0.0f, 0.0f, -1.0f };
		light.Color = color;
		light.Intensity = intensity;
		renderer.SubmitLight(light);
	}

	void LightFrom(Dingo::Renderer3D& renderer, const glm::vec3& toLight, float intensity)
	{
		renderer.SetAmbientLight(glm::vec3(0.0f), 1.0f);
		Dingo::DirectionalLight light;
		light.Direction = -glm::normalize(toLight);
		light.Color = glm::vec3(1.0f);
		light.Intensity = intensity;
		renderer.SubmitLight(light);
	}

	glm::vec3 PixelAt(const Dingo::TexturePixels& pixels, uint32_t x, uint32_t y)
	{
		return pixels.Data.empty() ? glm::vec3(-1.0f) : glm::vec3(pixels.GetPixel(x, y));
	}

	glm::vec3 CellCentre(const Dingo::TexturePixels& pixels, uint32_t column, uint32_t row)
	{
		return PixelAt(pixels, column * k_CellPixels + k_CellPixels / 2, row * k_CellPixels + k_CellPixels / 2);
	}

	float LargestDelta(const glm::vec3& a, const glm::vec3& b)
	{
		const glm::vec3 delta = glm::abs(a - b);
		return (std::max)((std::max)(delta.r, delta.g), delta.b);
	}

	uint32_t CountDiffering(const Dingo::TexturePixels& a, const Dingo::TexturePixels& b, float tolerance, float& worst, uint32_t firstX = 0, uint32_t endX = ~0u)
	{
		worst = 0.0f;
		if (a.Width != b.Width || a.Height != b.Height || a.Data.empty() || b.Data.empty())
			return ~0u;
		uint32_t differing = 0;
		for (uint32_t y = 0; y < a.Height; ++y)
		{
			for (uint32_t x = firstX; x < (std::min)(endX, a.Width); ++x)
			{
				const glm::vec4 delta = glm::abs(a.GetPixel(x, y) - b.GetPixel(x, y));
				const float largest = (std::max)((std::max)(delta.r, delta.g), (std::max)(delta.b, delta.a));
				worst = (std::max)(worst, largest);
				differing += largest > tolerance ? 1 : 0;
			}
		}
		return differing;
	}

	// What Renderer3D_Pbr.glsl writes at a texel where N = V = L = +z, so every dot product is 1.
	struct Surface
	{
		glm::vec3 Base{ 1.0f };
		glm::vec3 MeshColor{ 1.0f };
		float Metallic = 0.0f;
		float Roughness = 0.5f;
		glm::vec3 Emissive{ 0.0f };
		glm::vec3 Ambient{ 0.0f };
		glm::vec3 Light{ 0.0f };
	};

	glm::vec3 Reference(const Surface& surface, const glm::vec3& normal = glm::vec3(0.0f, 0.0f, 1.0f), const glm::vec3& toLight = glm::vec3(0.0f, 0.0f, 1.0f))
	{
		using namespace GlslBrdf;
		const glm::vec3 toCamera(0.0f, 0.0f, 1.0f);
		const vec3 base = SrgbToLinear(surface.MeshColor) * surface.Base;
		const float roughness = clamp(surface.Roughness, 0.045f, 1.0f);
		const float metallic = clamp(surface.Metallic, 0.0f, 1.0f);
		const vec3 diffuse = base * (1.0f - metallic);
		const vec3 F0 = mix(vec3(0.04f), base, metallic);
		const float a = roughness * roughness;

		const float NoV = clamp(glm::dot(normal, toCamera), 1e-4f, 1.0f);
		const float NoL = clamp(glm::dot(normal, toLight), 0.0f, 1.0f);
		const vec3 halfway = glm::normalize(toLight + toCamera);
		const float NoH = clamp(glm::dot(normal, halfway), 0.0f, 1.0f);
		const float VoH = clamp(glm::dot(toCamera, halfway), 0.0f, 1.0f);

		const vec3 F = F_Schlick(F0, VoH);
		const vec3 specular = F * (D_GGX(NoH, a) * V_SmithGGXCorrelated(NoV, NoL, a));
		const vec3 direct = ((vec3(1.0f) - F) * diffuse * (1.0f / DE_PI) + specular) * NoL;
		const vec3 ambient = SrgbToLinear(surface.Ambient) * (diffuse + EnvBRDFApprox(F0, roughness, NoV));
		return glm::clamp(LinearToSrgb(ambient + SrgbToLinear(surface.Light) * DE_PI * direct + surface.Emissive), 0.0f, 1.0f);
	}

	std::array<uint8_t, 4> NormalTexel(const glm::vec2& tilt)
	{
		const float z = std::sqrt(1.0f - tilt.x * tilt.x - tilt.y * tilt.y);
		auto encode = [](float value) { return static_cast<uint8_t>(std::lround((value * 0.5f + 0.5f) * 255.0f)); };
		return { encode(tilt.x), encode(tilt.y), encode(z), 255 };
	}

	// The world normal of a flat +z quad whose map holds `tilt` with u along +x and v along +y.
	glm::vec3 DecodedNormal(const glm::vec2& tilt)
	{
		const std::array<uint8_t, 4> texel = NormalTexel(tilt);
		return glm::normalize(glm::vec3(texel[0], texel[1], texel[2]) / 255.0f * 2.0f - 1.0f);
	}

	std::string Rgb(const glm::vec3& color)
	{
		return std::format("{:.3f} {:.3f} {:.3f}", color.r, color.g, color.b);
	}

	constexpr uint32_t k_ImportImageSize = 8;

	std::string Base64(const std::vector<uint8_t>& bytes)
	{
		static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		std::string text;
		for (size_t i = 0; i < bytes.size(); i += 3)
		{
			const uint32_t triple = (static_cast<uint32_t>(bytes[i]) << 16)
				| ((i + 1 < bytes.size() ? bytes[i + 1] : 0u) << 8)
				| (i + 2 < bytes.size() ? bytes[i + 2] : 0u);
			text += alphabet[(triple >> 18) & 63];
			text += alphabet[(triple >> 12) & 63];
			text += i + 1 < bytes.size() ? alphabet[(triple >> 6) & 63] : '=';
			text += i + 2 < bytes.size() ? alphabet[triple & 63] : '=';
		}
		return text;
	}

	// The left half of the image is `left`, the right half `right`.
	std::vector<uint8_t> SplitImage(const std::array<uint8_t, 4>& left, const std::array<uint8_t, 4>& right)
	{
		std::vector<uint8_t> pixels;
		for (uint32_t y = 0; y < k_ImportImageSize; ++y)
		{
			for (uint32_t x = 0; x < k_ImportImageSize; ++x)
			{
				const std::array<uint8_t, 4>& texel = x < k_ImportImageSize / 2 ? left : right;
				pixels.insert(pixels.end(), texel.begin(), texel.end());
			}
		}
		return pixels;
	}

	std::string PngDataUri(const std::filesystem::path& folder, const char* name, const std::vector<uint8_t>& rgba)
	{
		const std::filesystem::path file = folder / name;
		Dingo::FileSystem::WriteImage(file, k_ImportImageSize, k_ImportImageSize, 4, rgba.data());
		std::ifstream in(file, std::ios::binary);
		const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		return "data:image/png;base64," + Base64(bytes);
	}

	// One quad, shared by two meshes that differ only in material.
	std::string QuadBufferUri()
	{
		const float positions[] = { -0.5f, -0.5f, 0.0f,  0.5f, -0.5f, 0.0f,  0.5f, 0.5f, 0.0f,  -0.5f, 0.5f, 0.0f };
		const float normals[] = { 0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f };
		const float uvs[] = { 0.0f, 1.0f,  1.0f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f };
		const uint16_t indices[] = { 0, 1, 2, 2, 3, 0 };
		std::vector<uint8_t> bytes;
		auto append = [&bytes](const void* source, size_t size)
		{
			const uint8_t* begin = static_cast<const uint8_t*>(source);
			bytes.insert(bytes.end(), begin, begin + size);
		};
		append(positions, sizeof(positions));
		append(normals, sizeof(normals));
		append(uvs, sizeof(uvs));
		append(indices, sizeof(indices));
		return "data:application/octet-stream;base64," + Base64(bytes);
	}

	bool Near(float a, float b)
	{
		return std::abs(a - b) <= 1e-3f;
	}

	bool Near(const glm::vec4& a, const glm::vec4& b)
	{
		return Near(a.x, b.x) && Near(a.y, b.y) && Near(a.z, b.z) && Near(a.w, b.w);
	}

	bool Near(const glm::vec3& a, const glm::vec3& b)
	{
		return Near(a.x, b.x) && Near(a.y, b.y) && Near(a.z, b.z);
	}

	using Texel = std::array<uint8_t, 4>;

	constexpr Texel k_AlbedoLeft{ 200, 100, 50, 255 };
	constexpr Texel k_AlbedoRight{ 40, 20, 10, 255 };
	constexpr Texel k_NormalLeft{ 204, 128, 230, 255 };
	constexpr Texel k_NormalRight{ 51, 128, 230, 255 };
	constexpr Texel k_OrmLeft{ 255, 200, 100, 255 };
	constexpr Texel k_OrmRight{ 255, 100, 200, 255 };
	constexpr Texel k_EmissiveLeft{ 255, 128, 0, 255 };
	constexpr Texel k_EmissiveRight{ 0, 128, 255, 255 };

	// Within `tolerance` of the 0 to 255 texel.
	bool TexelNear(const Dingo::TexturePixels& image, uint32_t x, const Texel& expected, int tolerance)
	{
		if (image.Data.empty() || x >= image.Width)
			return false;
		const glm::vec4 read = image.GetPixel(x, 0) * 255.0f;
		for (int c = 0; c < 4; ++c)
		{
			if (std::abs(static_cast<int>(std::lround(read[c])) - expected[c]) > tolerance)
				return false;
		}
		return true;
	}

	std::string TexelText(const Dingo::TexturePixels& image, uint32_t x)
	{
		if (image.Data.empty() || x >= image.Width)
			return "no pixels";
		const glm::vec4 read = image.GetPixel(x, 0) * 255.0f;
		return std::format("{:.0f} {:.0f} {:.0f} {:.0f}", read.r, read.g, read.b, read.a);
	}

}

namespace Dingo
{

	Framebuffer* PbrMaterialTest::CreateTarget(uint32_t width, uint32_t height)
	{
		Framebuffer* target = Framebuffer::Create(FramebufferParams()
			.SetDebugName("PbrTest_Check")
			.SetWidth(static_cast<int32_t>(width))
			.SetHeight(static_cast<int32_t>(height))
			.SetEnableDepth(true)
			.AddAttachment({ TextureFormat::RGBA8_UNORM }));
		m_Targets.push_back(target);
		return target;
	}

	Material* PbrMaterialTest::CreatePbr(const PbrMaterialParams& params)
	{
		Material* material = m_Renderer->CreatePbrMaterial(params);
		if (material)
			m_Materials.push_back(material);
		return material;
	}

	template<typename Submit>
	Renderer3D::Statistics PbrMaterialTest::Render(Framebuffer* target, const glm::mat4& viewProjection, const glm::vec4& clear, Submit&& submit)
	{
		Framebuffer* previous = Renderer::GetRenderTarget();
		Renderer::SetRenderTarget(target);
		m_Renderer->BeginScene(viewProjection);
		m_Renderer->Clear(clear);
		submit(*m_Renderer);
		m_Renderer->EndScene();
		Renderer::SetRenderTarget(previous);
		return m_Renderer->GetStatistics();
	}

	void PbrMaterialTest::ReadAll(std::vector<Framebuffer*> targets, std::function<void(const std::vector<TexturePixels>&)> done)
	{
		auto images = std::make_shared<std::vector<TexturePixels>>(targets.size());
		auto remaining = std::make_shared<size_t>(targets.size());
		const std::weak_ptr<int> alive = m_Alive;
		for (size_t i = 0; i < targets.size(); ++i)
		{
			targets[i]->GetAttachment(0)->ReadPixels([alive, images, remaining, i, done](const TexturePixels& pixels)
			{
				if (alive.expired())
					return;
				(*images)[i] = pixels;
				if (--*remaining == 0)
					done(*images);
			});
		}
	}

	void PbrMaterialTest::Initialize()
	{
		m_Checks.clear();
		m_Steps.clear();
		m_NextStep = 0;
		m_Alive = std::make_shared<int>(0);

		m_Renderer = Renderer3D::Create();
		m_Font = Font::Create("fonts/Inter-Bold.ttf");
		m_ExactSphere = Mesh::CreateSphere(k_SphereRadius, k_ExactRings, k_ExactSegments);
		m_SmallSphere = Mesh::CreateSphere(k_SphereRadius, 16, 16);

		const std::vector<MeshVertex> vertices = {
			{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } },
			{ {  0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },
			{ {  0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },
			{ { -0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
		};
		m_Quad = Mesh::Create(vertices, { 0, 1, 2, 2, 3, 0 });

		const uint8_t halfAlpha[4] = { 255, 255, 255, 128 };
		m_HalfAlpha = Texture::CreateFromData(1, 1, halfAlpha, TextureFormat::RGBA8_UNORM, "PbrTest_HalfAlpha");

		for (uint32_t row = 0; row < k_GridCells; ++row)
		{
			for (uint32_t column = 0; column < k_GridCells; ++column)
			{
				m_GridMaterials.push_back(CreatePbr(PbrMaterialParams()
					.SetMaterial(MaterialParams().SetDebugName("PbrTest_Grid"))
					.SetBaseColor({ 1.0f, 0.0f, 0.0f, 1.0f })
					.SetMetallic(k_Metallic[row])
					.SetRoughness(k_Roughness[column])));
			}
		}

		{
			constexpr uint32_t size = 128;
			constexpr float cells = 4.0f;
			std::vector<uint8_t> bumps;
			for (uint32_t y = 0; y < size; ++y)
			{
				for (uint32_t x = 0; x < size; ++x)
				{
					const float a = 2.0f * glm::pi<float>() * cells * (static_cast<float>(x) + 0.5f) / size;
					const float b = 2.0f * glm::pi<float>() * cells * (static_cast<float>(y) + 0.5f) / size;
					const std::array<uint8_t, 4> texel = NormalTexel(0.45f * glm::vec2(-std::cos(a) * std::sin(b), -std::sin(a) * std::cos(b)));
					bumps.insert(bumps.end(), texel.begin(), texel.end());
				}
			}
			Texture* bumpMap = Texture::CreateFromData(size, size, bumps.data(), TextureFormat::RGBA8_UNORM, "PbrTest_BumpMap");
			m_Textures.push_back(bumpMap);
			m_RepeatSampler = Sampler::Create(SamplerParams().SetAddressMode(SamplerAddressMode::Repeat));

			const float scales[3] = { 1.0f, 1.0f, 2.0f };
			for (uint32_t i = 0; i < 3; ++i)
			{
				PbrMaterialParams params = PbrMaterialParams()
					.SetMaterial(MaterialParams().SetDebugName("PbrTest_Panel"))
					.SetBaseColor({ 0.8f, 0.75f, 0.7f, 1.0f })
					.SetRoughness(0.5f);
				if (i > 0)
					params.SetNormalMap(bumpMap).SetNormalScale(scales[i]).SetSampler(m_RepeatSampler);
				m_PanelMaterials[i] = CreatePbr(params);
			}
		}

		if (Model* fox = Model::LoadFromFile("models/Fox/Fox.gltf"); fox && fox->IsSkinned())
		{
			m_Fox = fox;
			glm::vec3 low((std::numeric_limits<float>::max)());
			glm::vec3 high((std::numeric_limits<float>::lowest)());
			Texture* albedo = nullptr;
			for (const SubMesh& submesh : m_Fox->GetSubMeshes())
			{
				for (const MeshVertex& vertex : submesh.Mesh->GetVertices())
				{
					low = (glm::min)(low, vertex.Position);
					high = (glm::max)(high, vertex.Position);
				}
				if (!albedo)
					albedo = submesh.DiffuseTexture;
			}
			const glm::vec3 extent = high - low;
			const float scale = 2.0f / (std::max)({ extent.x, extent.y, extent.z });
			m_FoxTransform = glm::rotate(glm::mat4(1.0f), extent.x >= extent.z ? 0.0f : glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f))
				* glm::scale(glm::mat4(1.0f), glm::vec3(scale))
				* glm::translate(glm::mat4(1.0f), -0.5f * (low + high));
			m_FoxMaterial = CreatePbr(PbrMaterialParams().SetMaterial(MaterialParams().SetDebugName("PbrTest_Fox")).SetAlbedoMap(albedo).SetRoughness(0.6f));
			m_FoxPalette.assign(m_Fox->GetSkeleton()->GetRestPalette().begin(), m_Fox->GetSkeleton()->GetRestPalette().end());
		}
		else
		{
			delete fox;
		}

		m_Steps = {
			[this] { CheckGrid(); },
			[this] { CheckMixed(); },
			[this] { CheckCoexistence(); },
			[this] { CheckFox(); },
			[this] { CheckInstancing(); },
			[this] { CheckStaticBatch(); },
			[this] { CheckSurfaceOverrides(); },
			[this] { CheckTranslucency(); },
			[this] { CheckPastLastSlot(); },
			[this] { CheckNormalMap(); },
			[this] { CheckImport(); },
			[this] { CheckSceneSkinned(); },
		};
	}

	void PbrMaterialTest::SubmitGrid(Renderer3D& renderer)
	{
		for (uint32_t row = 0; row < k_GridCells; ++row)
		{
			for (uint32_t column = 0; column < k_GridCells; ++column)
				renderer.SubmitMesh(m_ExactSphere, CellAt(k_GridCells, k_GridCells, column, row), glm::vec4(1.0f), m_GridMaterials[row * k_GridCells + column]);
		}
	}

	void PbrMaterialTest::CheckGrid()
	{
		Framebuffer* target = CreateTarget(k_GridCells * k_CellPixels, k_GridCells * k_CellPixels);
		const Renderer3D::Statistics stats = Render(target, ViewProjection(k_GridCells, k_GridCells), k_Black, [this](Renderer3D& renderer)
		{
			LightFromCamera(renderer, glm::vec3(1.0f), k_LightIntensity, glm::vec3(0.0f));
			SubmitGrid(renderer);
		});
		Framebuffer* peakTarget = CreateTarget(k_GridCells * k_CellPixels, k_GridCells * k_CellPixels);
		Render(peakTarget, ViewProjection(k_GridCells, k_GridCells), k_Black, [this](Renderer3D& renderer)
		{
			LightFromCamera(renderer, glm::vec3(1.0f), k_PeakLightIntensity, glm::vec3(0.0f));
			SubmitGrid(renderer);
		});
		Check(stats.PbrDraws == k_GridCells * k_GridCells && stats.DroppedMeshes == 0,
			std::format("PBR 1: the 5 x 5 grid draws one PBR draw per sphere ({} PBR draws, {} dropped)", stats.PbrDraws, stats.DroppedMeshes));

		ReadAll({ target, peakTarget }, [this](const std::vector<TexturePixels>& images)
		{
			const TexturePixels& image = images[0];
			float worst = 0.0f;
			std::string worstAt;
			for (uint32_t row = 0; row < k_GridCells; ++row)
			{
				for (uint32_t column = 0; column < k_GridCells; ++column)
				{
					Surface surface;
					surface.Base = { 1.0f, 0.0f, 0.0f };
					surface.Metallic = k_Metallic[row];
					surface.Roughness = k_Roughness[column];
					surface.Light = glm::vec3(k_LightIntensity);
					const glm::vec3 expected = Reference(surface);
					const glm::vec3 read = CellCentre(image, column, row);
					const float delta = LargestDelta(read, expected);
					if (delta > worst || worstAt.empty())
					{
						worst = (std::max)(worst, delta);
						if (delta >= worst)
							worstAt = std::format("metallic {:.2f} roughness {:.2f}: read {}, expected {}", k_Metallic[row], k_Roughness[column], Rgb(read), Rgb(expected));
					}
				}
			}
			Check(worst <= k_Tolerance,
				std::format("PBR 1: every sphere's centre texel matches the CPU evaluation of Brdf.glsl (worst {:.1f}/255, {})", worst * 255.0f, worstAt));

			float peakWorst = 0.0f;
			float peakBrightest = 0.0f;
			float peakDimmest = 1.0f;
			std::string peakAt;
			for (uint32_t row = 0; row < k_GridCells; ++row)
			{
				Surface surface;
				surface.Base = { 1.0f, 0.0f, 0.0f };
				surface.Metallic = k_Metallic[row];
				surface.Roughness = k_Roughness[0];
				surface.Light = glm::vec3(k_PeakLightIntensity);
				const glm::vec3 expected = Reference(surface);
				const glm::vec3 read = CellCentre(images[1], 0, row);
				peakBrightest = (std::max)(peakBrightest, read.r);
				peakDimmest = (std::min)(peakDimmest, read.r);
				const float delta = LargestDelta(read, expected);
				if (delta >= peakWorst)
				{
					peakWorst = delta;
					peakAt = std::format("metallic {:.2f}: read {}, expected {}", k_Metallic[row], Rgb(read), Rgb(expected));
				}
			}
			Check(peakWorst <= k_Tolerance && peakBrightest < 0.95f && peakDimmest > 0.05f,
				std::format("PBR 1: at a light dim enough that the roughness-0.1 peak does not clip, its centre texels match the CPU evaluation (worst {:.1f}/255, red {:.3f} to {:.3f}, {})",
					peakWorst * 255.0f, peakDimmest, peakBrightest, peakAt));

			std::string fault;
			for (uint32_t row = 0; row < k_GridCells && fault.empty(); ++row)
			{
				bool falls = true;
				for (uint32_t column = 1; column < k_GridCells; ++column)
					falls = falls && CellCentre(image, column, row).r <= CellCentre(image, column - 1, row).r + 1.0f / 255.0f;
				const float first = CellCentre(image, 0, row).r;
				const float last = CellCentre(image, k_GridCells - 1, row).r;
				if (!falls || first - last < 4.0f / 255.0f)
					fault = std::format("metallic {:.2f}: red {:.3f} at roughness {:.2f}, {:.3f} at {:.2f}", k_Metallic[row], first, k_Roughness[0], last, k_Roughness[k_GridCells - 1]);
			}
			Check(fault.empty(), std::format("PBR 1: the highlight peak falls as roughness rises along every row ({})", fault.empty() ? "all five rows" : fault));

			float most = 0.0f;
			float dimmest = 1.0f;
			for (uint32_t column = 0; column < k_GridCells; ++column)
			{
				const glm::vec3 metal = CellCentre(image, column, k_GridCells - 1);
				most = (std::max)(most, (std::max)(metal.g, metal.b));
				dimmest = (std::min)(dimmest, metal.r);
			}
			Check(most <= 1.0f / 255.0f && dimmest > 0.1f,
				std::format("PBR 1: a fully metallic sphere with a pure red base colour lights no green or blue (green/blue at most {:.1f}/255, red at least {:.3f})", most * 255.0f, dimmest));
		});
	}

	void PbrMaterialTest::CheckMixed()
	{
		const glm::vec3 base{ 0.9f, 0.5f, 0.2f };
		const glm::vec3 meshColor{ 0.5f, 0.8f, 1.0f };
		const glm::vec3 emissive{ 0.1f, 0.05f, 0.0f };
		const glm::vec3 ambient{ 0.3f, 0.4f, 0.5f };
		const glm::vec3 lightColor{ 1.0f, 0.9f, 0.8f };
		constexpr float strength = 2.0f;
		constexpr float intensity = 0.3f;

		Material* material = CreatePbr(PbrMaterialParams()
			.SetMaterial(MaterialParams().SetDebugName("PbrTest_Mixed"))
			.SetBaseColor(glm::vec4(base, 1.0f))
			.SetMetallic(0.5f)
			.SetRoughness(0.4f)
			.SetEmissive(emissive)
			.SetEmissiveStrength(strength));
		Framebuffer* target = CreateTarget(k_CellPixels, k_CellPixels);
		Render(target, ViewProjection(1, 1), k_Black, [&](Renderer3D& renderer)
		{
			LightFromCamera(renderer, lightColor, intensity, ambient);
			renderer.SubmitMesh(m_ExactSphere, glm::mat4(1.0f), glm::vec4(meshColor, 1.0f), material);
		});

		ReadAll({ target }, [=, this](const std::vector<TexturePixels>& images)
		{
			Surface surface;
			surface.Base = base;
			surface.MeshColor = meshColor;
			surface.Metallic = 0.5f;
			surface.Roughness = 0.4f;
			surface.Emissive = emissive * strength;
			surface.Ambient = ambient;
			surface.Light = lightColor * intensity;
			const glm::vec3 expected = Reference(surface);
			const glm::vec3 read = CellCentre(images[0], 0, 0);
			Check(LargestDelta(read, expected) <= k_Tolerance,
				std::format("PBR 1: with an ambient light, an emissive colour and a tinted mesh colour the centre texel matches the CPU evaluation (read {}, expected {})", Rgb(read), Rgb(expected)));
		});
	}

	void PbrMaterialTest::CheckCoexistence()
	{
		Material* lit = m_Renderer->CreateLitMaterial(MaterialParams().SetDebugName("PbrTest_Lit"));
		m_Materials.push_back(lit);
		Material* pbr = CreatePbr(PbrMaterialParams().SetMaterial(MaterialParams().SetDebugName("PbrTest_Beside")).SetBaseColor({ 0.2f, 0.6f, 0.9f, 1.0f }).SetRoughness(0.4f));

		Framebuffer* alone = CreateTarget(2 * k_CellPixels, k_CellPixels);
		Framebuffer* beside = CreateTarget(2 * k_CellPixels, k_CellPixels);
		auto scene = [&](bool withPbr)
		{
			return [&, withPbr](Renderer3D& renderer)
			{
				LightFromCamera(renderer, glm::vec3(1.0f), 0.6f, glm::vec3(0.15f));
				renderer.SubmitMesh(m_SmallSphere, CellAt(2, 1, 0, 0), { 0.9f, 0.4f, 0.3f, 1.0f }, lit);
				if (withPbr)
					renderer.SubmitMesh(m_SmallSphere, CellAt(2, 1, 1, 0), glm::vec4(1.0f), pbr);
			};
		};
		Render(alone, ViewProjection(2, 1), k_Black, scene(false));
		const Renderer3D::Statistics stats = Render(beside, ViewProjection(2, 1), k_Black, scene(true));
		Check(stats.PbrDraws == 1 && stats.DrawCalls == 2,
			std::format("PBR 2: the scene with a PBR mesh draws it beside the lit one ({} PBR draws, {} draw calls)", stats.PbrDraws, stats.DrawCalls));

		ReadAll({ alone, beside }, [this](const std::vector<TexturePixels>& images)
		{
			float worst = 0.0f;
			float unused = 0.0f;
			const uint32_t litDiffering = CountDiffering(images[0], images[1], 0.0f, worst, 0, k_CellPixels);
			const uint32_t pbrDiffering = CountDiffering(images[0], images[1], 0.0f, unused, k_CellPixels);
			Check(litDiffering == 0, std::format("PBR 2: a lit material's half of the picture is the same with and without a PBR mesh in the scene ({} px differ, by up to {:.0f}/255)", litDiffering, worst * 255.0f));
			Check(pbrDiffering > 100 && pbrDiffering != ~0u, std::format("PBR 2: the PBR mesh does draw in its half ({} px)", pbrDiffering));
		});
	}

	void PbrMaterialTest::CheckFox()
	{
		if (!m_Fox || !m_FoxMaterial)
		{
			Check(false, "PBR 4: the Fox model loads as a skinned model");
			return;
		}

		const glm::mat4 viewProjection = glm::ortho(-1.2f, 1.2f, -1.2f, 1.2f, -5.0f, 5.0f);
		Framebuffer* skinned = CreateTarget(128, 128);
		Framebuffer* rigid = CreateTarget(128, 128);
		auto light = [](Renderer3D& renderer) { LightFromCamera(renderer, glm::vec3(1.0f), 0.8f, glm::vec3(0.1f)); };
		const Renderer3D::Statistics stats = Render(skinned, viewProjection, k_Backdrop, [&](Renderer3D& renderer)
		{
			light(renderer);
			for (const SubMesh& submesh : m_Fox->GetSubMeshes())
			{
				if (submesh.Mesh->HasSkin())
					renderer.SubmitSkinnedMesh(submesh.Mesh, m_FoxTransform, m_FoxPalette, glm::vec4(1.0f), m_FoxMaterial);
			}
		});
		Render(rigid, viewProjection, k_Backdrop, [&](Renderer3D& renderer)
		{
			light(renderer);
			for (const SubMesh& submesh : m_Fox->GetSubMeshes())
			{
				if (submesh.Mesh->HasSkin())
					renderer.SubmitMesh(submesh.Mesh, m_FoxTransform, glm::vec4(1.0f), m_FoxMaterial);
			}
		});
		Check(stats.SkinnedDraws > 0 && stats.PbrDraws > 0,
			std::format("PBR 4: the skinned Fox draws through the skinned PBR path ({} skinned draws, {} PBR draws)", stats.SkinnedDraws, stats.PbrDraws));

		ReadAll({ skinned, rigid }, [this](const std::vector<TexturePixels>& images)
		{
			uint32_t covered = 0;
			for (uint32_t y = 0; y < images[0].Height; ++y)
			{
				for (uint32_t x = 0; x < images[0].Width; ++x)
					covered += LargestDelta(PixelAt(images[0], x, y), glm::vec3(k_Backdrop)) > 8.0f / 255.0f ? 1 : 0;
			}
			float worst = 0.0f;
			const uint32_t differing = CountDiffering(images[0], images[1], k_Tolerance, worst);
			Check(covered > 500 && differing == 0,
				std::format("PBR 4: at its rest pose the skinned Fox matches the same mesh drawn through SubmitMesh ({} px differ by more than 2/255, up to {:.1f}/255; the Fox covers {} px)",
					differing, worst * 255.0f, covered));
		});
	}

	void PbrMaterialTest::CheckInstancing()
	{
		constexpr uint32_t cols = 4;
		constexpr uint32_t rows = 3;
		Material* material = CreatePbr(PbrMaterialParams().SetMaterial(MaterialParams().SetDebugName("PbrTest_Instanced")).SetBaseColor({ 0.9f, 0.9f, 0.9f, 1.0f }).SetMetallic(0.3f).SetRoughness(0.5f));
		auto copies = [=, this](Renderer3D& renderer)
		{
			LightFromCamera(renderer, glm::vec3(1.0f), 0.5f, glm::vec3(0.1f));
			for (uint32_t i = 0; i < cols * rows; ++i)
			{
				const float t = static_cast<float>(i) / static_cast<float>(cols * rows - 1);
				renderer.SubmitMesh(m_SmallSphere, CellAt(cols, rows, i % cols, i / cols), { 1.0f - t, 0.3f + 0.5f * t, t, 1.0f }, material);
			}
		};

		Framebuffer* instanced = CreateTarget(cols * k_CellPixels, rows * k_CellPixels);
		Framebuffer* batched = CreateTarget(cols * k_CellPixels, rows * k_CellPixels);
		const Renderer3D::Statistics stats = Render(instanced, ViewProjection(cols, rows), k_Black, copies);
		m_Renderer->SetInstancingEnabled(false);
		const Renderer3D::Statistics cpu = Render(batched, ViewProjection(cols, rows), k_Black, copies);
		m_Renderer->SetInstancingEnabled(true);
		Check(stats.InstancedDraws == 1 && stats.InstancedMeshes == cols * rows && stats.PbrDraws >= 1 && cpu.InstancedMeshes == 0 && cpu.PbrDraws >= 1,
			std::format("PBR 5: {} copies of one PBR mesh are one instanced draw, and batched with instancing off ({} instanced draws, {} instanced; off: {} instanced, {} PBR draws)",
				cols * rows, stats.InstancedDraws, stats.InstancedMeshes, cpu.InstancedMeshes, cpu.PbrDraws));

		ReadAll({ instanced, batched }, [this](const std::vector<TexturePixels>& images)
		{
			float worst = 0.0f;
			const uint32_t differing = CountDiffering(images[0], images[1], k_InstanceTolerance, worst);
			Check(differing == 0, std::format("PBR 5: the instanced copies match the same copies batched on the CPU ({} px differ by more than 2.5/255, up to {:.1f}/255)", differing, worst * 255.0f));
		});
	}

	void PbrMaterialTest::CheckStaticBatch()
	{
		constexpr uint32_t cols = 4;
		constexpr uint32_t rows = 3;
		Material* material = CreatePbr(PbrMaterialParams().SetMaterial(MaterialParams().SetDebugName("PbrTest_Static")).SetBaseColor({ 0.8f, 0.8f, 0.4f, 1.0f }).SetMetallic(0.6f).SetRoughness(0.35f));
		auto colorOf = [=](uint32_t i)
		{
			const float t = static_cast<float>(i) / static_cast<float>(cols * rows - 1);
			return glm::vec4(0.3f + 0.7f * t, 1.0f - t, 0.6f, 1.0f);
		};
		auto lights = [](Renderer3D& renderer) { LightFromCamera(renderer, glm::vec3(1.0f), 0.5f, glm::vec3(0.1f)); };

		Framebuffer* direct = CreateTarget(cols * k_CellPixels, rows * k_CellPixels);
		Framebuffer* baked = CreateTarget(cols * k_CellPixels, rows * k_CellPixels);
		m_Renderer->SetInstancingEnabled(false);
		Render(direct, ViewProjection(cols, rows), k_Black, [&](Renderer3D& renderer)
		{
			lights(renderer);
			for (uint32_t i = 0; i < cols * rows; ++i)
				renderer.SubmitMesh(m_SmallSphere, CellAt(cols, rows, i % cols, i / cols), colorOf(i), material);
		});
		m_Renderer->SetInstancingEnabled(true);

		m_StaticBatch = m_Renderer->CreateStaticBatch();
		for (uint32_t i = 0; i < cols * rows; ++i)
			m_StaticBatch->Add(StaticMesh().SetMesh(m_SmallSphere).SetTransform(CellAt(cols, rows, i % cols, i / cols)).SetColor(colorOf(i)).SetMaterial(material));
		const Renderer3D::Statistics stats = Render(baked, ViewProjection(cols, rows), k_Black, [&](Renderer3D& renderer)
		{
			lights(renderer);
			renderer.SubmitStaticBatch(*m_StaticBatch);
		});
		Check(stats.StaticMeshes == cols * rows && stats.PbrDraws >= 1,
			std::format("PBR 6: a static batch submits its {} PBR meshes ({} static, {} PBR draws)", cols * rows, stats.StaticMeshes, stats.PbrDraws));

		ReadAll({ direct, baked }, [this](const std::vector<TexturePixels>& images)
		{
			float worst = 0.0f;
			const uint32_t differing = CountDiffering(images[0], images[1], 0.0f, worst);
			Check(differing == 0, std::format("PBR 6: a PBR static batch matches SubmitMesh ({} px differ, by up to {:.1f}/255)", differing, worst * 255.0f));
		});
	}

	void PbrMaterialTest::CheckSurfaceOverrides()
	{
		const glm::vec3 base{ 0.9f, 0.6f, 0.3f };
		const SurfaceOverride overrides[2] = {
			SurfaceOverride().SetRoughness(0.5f).SetMetallic(0.0f),
			SurfaceOverride().SetRoughness(0.9f).SetMetallic(1.0f),
		};
		Material* material = CreatePbr(PbrMaterialParams()
			.SetMaterial(MaterialParams().SetDebugName("PbrTest_Override"))
			.SetBaseColor(glm::vec4(base, 1.0f))
			.SetMetallic(0.2f)
			.SetRoughness(0.3f));

		Framebuffer* target = CreateTarget(2 * k_CellPixels, k_CellPixels);
		m_Renderer->SetInstancingEnabled(false);
		const Renderer3D::Statistics stats = Render(target, ViewProjection(2, 1), k_Black, [&](Renderer3D& renderer)
		{
			LightFromCamera(renderer, glm::vec3(1.0f), k_LightIntensity, glm::vec3(0.0f));
			for (uint32_t i = 0; i < 2; ++i)
				renderer.SubmitMesh(m_ExactSphere, CellAt(2, 1, i, 0), glm::vec4(1.0f), material, ShadowCasting::On, 1, 0.0f, &overrides[i]);
		});
		m_Renderer->SetInstancingEnabled(true);
		Check(stats.SurfaceOverrides == 2 && stats.DrawCalls == 1 && stats.PbrDraws == 1,
			std::format("PBR 7: two meshes with different surfaces share one PBR draw and count as overrides ({} overrides, {} draw calls, {} PBR draws)", stats.SurfaceOverrides, stats.DrawCalls, stats.PbrDraws));

		ReadAll({ target }, [=, this](const std::vector<TexturePixels>& images)
		{
			glm::vec3 read[2];
			glm::vec3 expected[2];
			float worst = 0.0f;
			for (uint32_t i = 0; i < 2; ++i)
			{
				Surface surface;
				surface.Base = base;
				surface.Metallic = overrides[i].Metallic;
				surface.Roughness = overrides[i].Roughness;
				surface.Light = glm::vec3(k_LightIntensity);
				expected[i] = Reference(surface);
				read[i] = CellCentre(images[0], i, 0);
				worst = (std::max)(worst, LargestDelta(read[i], expected[i]));
			}
			Check(LargestDelta(read[0], read[1]) > 8.0f / 255.0f,
				std::format("PBR 7: the two surfaces highlight differently (read {} and {})", Rgb(read[0]), Rgb(read[1])));
			Check(worst <= k_Tolerance,
				std::format("PBR 7: each mesh reads its own override, not its material's (worst {:.1f}/255; read {} and {}, expected {} and {})",
					worst * 255.0f, Rgb(read[0]), Rgb(read[1]), Rgb(expected[0]), Rgb(expected[1])));
		});
	}

	void PbrMaterialTest::CheckTranslucency()
	{
		struct Variant
		{
			const char* Name;
			bool Translucent;
			float MeshAlpha;
			float BaseAlpha;
			bool HalfAlbedo;
			float Expected;
		};
		const float albedoAlpha = 128.0f / 255.0f;
		const Variant variants[] = {
			{ "translucent, BaseColor alpha", true, 1.0f, 0.5f, false, 0.5f },
			{ "translucent, albedo alpha", true, 1.0f, 1.0f, true, albedoAlpha },
			{ "translucent, mesh alpha", true, 0.8f, 1.0f, false, 0.8f },
			{ "translucent, all three", true, 0.8f, 0.5f, true, 0.8f * 0.5f * albedoAlpha },
			{ "opaque, all three (the mesh's alone)", false, 0.6f, 0.5f, true, 0.6f },
		};

		auto draw = [this](Material* material, float meshAlpha)
		{
			return [this, material, meshAlpha](Renderer3D& renderer)
			{
				LightFromCamera(renderer, glm::vec3(1.0f), 0.3f, glm::vec3(0.0f));
				renderer.SubmitMesh(m_Quad, glm::scale(glm::mat4(1.0f), { 3.0f, 3.0f, 1.0f }), { 1.0f, 1.0f, 1.0f, meshAlpha }, material);
			};
		};
		auto make = [this](bool translucent, float baseAlpha, bool halfAlbedo)
		{
			return CreatePbr(PbrMaterialParams()
				.SetMaterial(MaterialParams().SetDebugName("PbrTest_Alpha").SetTranslucent(translucent))
				.SetBaseColor({ 0.8f, 0.5f, 0.3f, baseAlpha })
				.SetRoughness(0.5f)
				.SetAlbedoMap(halfAlbedo ? m_HalfAlpha : nullptr));
		};

		const glm::mat4 viewProjection = ViewProjection(2, 2);
		std::vector<Framebuffer*> targets;
		targets.push_back(CreateTarget(16, 16));
		Render(targets[0], viewProjection, k_Backdrop, draw(make(false, 1.0f, false), 1.0f));
		for (const Variant& variant : variants)
		{
			targets.push_back(CreateTarget(16, 16));
			Render(targets.back(), viewProjection, k_Backdrop, draw(make(variant.Translucent, variant.BaseAlpha, variant.HalfAlbedo), variant.MeshAlpha));
		}

		ReadAll(targets, [=, this](const std::vector<TexturePixels>& images)
		{
			const glm::vec3 surface = PixelAt(images[0], 8, 8);
			for (size_t i = 0; i < std::size(variants); ++i)
			{
				const glm::vec3 expected = surface * variants[i].Expected + glm::vec3(k_Backdrop) * (1.0f - variants[i].Expected);
				const glm::vec3 read = PixelAt(images[i + 1], 8, 8);
				Check(LargestDelta(read, expected) <= k_Tolerance,
					std::format("PBR 8: {} blends over the backdrop with alpha {:.3f} (read {}, expected {})", variants[i].Name, variants[i].Expected, Rgb(read), Rgb(expected)));
			}
		});
	}

	void PbrMaterialTest::CheckPastLastSlot()
	{
		Material* material = CreatePbr(PbrMaterialParams().SetMaterial(MaterialParams().SetDebugName("PbrTest_Slot5")));
		material->SetTexture(5, Renderer::GetWhiteTexture());

		Framebuffer* target = CreateTarget(16, 16);
		const Renderer3D::Statistics stats = Render(target, ViewProjection(2, 2), k_Backdrop, [&](Renderer3D& renderer)
		{
			LightFromCamera(renderer, glm::vec3(1.0f), 0.3f, glm::vec3(0.0f));
			renderer.SubmitMesh(m_Quad, glm::scale(glm::mat4(1.0f), { 3.0f, 3.0f, 1.0f }), glm::vec4(1.0f), material);
		});
		Check(stats.PbrDraws == 0 && stats.DrawCalls == 0,
			std::format("PBR 9: a PBR material with a texture in slot 5 draws nothing ({} PBR draws, {} draw calls)", stats.PbrDraws, stats.DrawCalls));
		ReadAll({ target }, [this](const std::vector<TexturePixels>& images)
		{
			const glm::vec3 read = PixelAt(images[0], 8, 8);
			Check(LargestDelta(read, glm::vec3(k_Backdrop)) <= 1.0f / 255.0f, std::format("PBR 9: the picture is the backdrop alone (read {})", Rgb(read)));
		});
	}

	void PbrMaterialTest::CheckNormalMap()
	{
		constexpr float tilt = 0.6f;
		struct Case
		{
			const char* Name;
			glm::vec2 Tilt;
			glm::vec3 ToLight;
			bool Brighter;
		};
		const Case cases[4] = {
			{ "tilted toward +u, lit from +u", { tilt, 0.0f }, { 1.0f, 0.0f, 1.0f }, true },
			{ "tilted toward +u, lit from -u", { tilt, 0.0f }, { -1.0f, 0.0f, 1.0f }, false },
			{ "tilted toward +v, lit from +v", { 0.0f, tilt }, { 0.0f, 1.0f, 1.0f }, true },
			{ "tilted toward +v, lit from -v", { 0.0f, tilt }, { 0.0f, -1.0f, 1.0f }, false },
		};

		auto makeMesh = [this](bool flipU, bool flipV, bool noUv)
		{
			const glm::vec2 corners[4] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
			std::vector<MeshVertex> vertices;
			for (const glm::vec2& corner : corners)
			{
				const glm::vec2 uv = noUv ? glm::vec2(0.0f) : glm::vec2(flipU ? 1.0f - corner.x : corner.x, flipV ? 1.0f - corner.y : corner.y);
				vertices.push_back({ { corner.x - 0.5f, corner.y - 0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, uv });
			}
			Mesh* mesh = Mesh::Create(vertices, { 0, 1, 2, 2, 3, 0 });
			m_Meshes.push_back(mesh);
			return mesh;
		};
		auto makeMaterial = [this](const glm::vec2* mapTilt)
		{
			PbrMaterialParams params = PbrMaterialParams()
				.SetMaterial(MaterialParams().SetDebugName("PbrTest_Normal"))
				.SetBaseColor({ 0.4f, 0.4f, 0.4f, 1.0f })
				.SetRoughness(0.6f);
			if (mapTilt)
			{
				const std::array<uint8_t, 4> texel = NormalTexel(*mapTilt);
				Texture* map = Texture::CreateFromData(1, 1, texel.data(), TextureFormat::RGBA8_UNORM, "PbrTest_NormalMap");
				m_Textures.push_back(map);
				params.SetNormalMap(map);
			}
			return CreatePbr(params);
		};

		Mesh* plain = makeMesh(false, false, false);
		Mesh* mirroredU = makeMesh(true, false, false);
		Mesh* mirroredV = makeMesh(false, true, false);
		Mesh* withoutUv = makeMesh(false, false, true);
		Material* flat = makeMaterial(nullptr);

		std::vector<Framebuffer*> targets;
		auto draw = [&](Mesh* mesh, Material* material, const glm::vec3& toLight)
		{
			Framebuffer* target = CreateTarget(16, 16);
			Render(target, ViewProjection(1, 1), k_Black, [&](Renderer3D& renderer)
			{
				LightFrom(renderer, toLight, 1.0f);
				renderer.SubmitMesh(mesh, glm::scale(glm::mat4(1.0f), { 2.0f, 2.0f, 1.0f }), glm::vec4(1.0f), material);
			});
			targets.push_back(target);
		};

		for (const Case& c : cases)
		{
			const bool alongU = c.Tilt.x != 0.0f;
			const glm::vec2 mirroredTilt = alongU ? glm::vec2(-c.Tilt.x, 0.0f) : glm::vec2(0.0f, -c.Tilt.y);
			draw(plain, makeMaterial(&c.Tilt), c.ToLight);
			draw(plain, flat, c.ToLight);
			draw(alongU ? mirroredU : mirroredV, makeMaterial(&mirroredTilt), c.ToLight);
		}
		draw(withoutUv, makeMaterial(&cases[0].Tilt), cases[0].ToLight);

		ReadAll(targets, [=, this](const std::vector<TexturePixels>& images)
		{
			Surface surface;
			surface.Base = glm::vec3(0.4f);
			surface.Roughness = 0.6f;
			surface.Light = glm::vec3(1.0f);

			auto worstAgainst = [](const TexturePixels& image, const glm::vec3& expected)
			{
				if (image.Data.empty())
					return 1.0f;
				float worst = 0.0f;
				for (uint32_t y = 0; y < image.Height; ++y)
				{
					for (uint32_t x = 0; x < image.Width; ++x)
						worst = (std::max)(worst, LargestDelta(glm::vec3(image.GetPixel(x, y)), expected));
				}
				return worst;
			};

			for (size_t i = 0; i < std::size(cases); ++i)
			{
				const Case& c = cases[i];
				const TexturePixels& mapped = images[i * 3];
				const TexturePixels& unmapped = images[i * 3 + 1];
				const TexturePixels& mirrored = images[i * 3 + 2];
				const glm::vec3 toLight = glm::normalize(c.ToLight);
				const glm::vec3 expected = Reference(surface, DecodedNormal(c.Tilt), toLight);
				const glm::vec3 expectedFlat = Reference(surface, glm::vec3(0.0f, 0.0f, 1.0f), toLight);
				const glm::vec3 read = PixelAt(mapped, 8, 8);
				const glm::vec3 readFlat = PixelAt(unmapped, 8, 8);

				const float worst = worstAgainst(mapped, expected);
				Check(worst <= k_Tolerance && worstAgainst(unmapped, expectedFlat) <= k_Tolerance,
					std::format("PBR 3: a normal map {} matches the CPU evaluation (worst {:.1f}/255; read {}, expected {}; without the map read {}, expected {})",
						c.Name, worst * 255.0f, Rgb(read), Rgb(expected), Rgb(readFlat), Rgb(expectedFlat)));
				const float change = read.r - readFlat.r;
				Check(c.Brighter ? change > 4.0f / 255.0f : change < -4.0f / 255.0f,
					std::format("PBR 3: the map ({}) makes the quad {} than the same quad without one ({:.3f} against {:.3f})",
						c.Name, c.Brighter ? "brighter" : "darker", read.r, readFlat.r));

				float mirroredWorst = 0.0f;
				const uint32_t differing = CountDiffering(mapped, mirrored, 2.0f / 255.0f, mirroredWorst);
				Check(differing == 0,
					std::format("PBR 3: {}, drawn with {} mirrored and the map flipped to match, is the same picture ({} texels differ, worst {:.1f}/255)",
						c.Name, c.Tilt.x != 0.0f ? "u" : "v", differing, mirroredWorst * 255.0f));
			}

			const TexturePixels& bare = images[3 * std::size(cases)];
			const glm::vec3 expectedFlat = Reference(surface, glm::vec3(0.0f, 0.0f, 1.0f), glm::normalize(cases[0].ToLight));
			const float worst = worstAgainst(bare, expectedFlat);
			float darkest = 1.0f;
			for (uint32_t y = 0; y < bare.Height && !bare.Data.empty(); ++y)
			{
				for (uint32_t x = 0; x < bare.Width; ++x)
				{
					const glm::vec3 texel(bare.GetPixel(x, y));
					darkest = (std::min)(darkest, (std::min)(texel.r, (std::min)(texel.g, texel.b)));
				}
			}
			Check(worst <= k_Tolerance && darkest > 0.1f,
				std::format("PBR 3: a mesh without UVs keeps its geometric normal under a bound map, with no black texels (worst {:.1f}/255, darkest channel {:.3f}, expected {})",
					worst * 255.0f, darkest, Rgb(expectedFlat)));
		});
	}

	void PbrMaterialTest::CheckImport()
	{
		const std::filesystem::path folder = TestTempDirectory("PbrImport");
		std::filesystem::create_directories(folder);
		m_ImportFolder = folder;

		const std::string images[4] = {
			PngDataUri(folder, "albedo.png", SplitImage(k_AlbedoLeft, k_AlbedoRight)),
			PngDataUri(folder, "normal.png", SplitImage(k_NormalLeft, k_NormalRight)),
			PngDataUri(folder, "orm.png", SplitImage(k_OrmLeft, k_OrmRight)),
			PngDataUri(folder, "emissive.png", SplitImage(k_EmissiveLeft, k_EmissiveRight)),
		};
		std::string json = R"({"asset":{"version":"2.0"},"extensionsUsed":["KHR_materials_emissive_strength"],"scene":0,"scenes":[{"nodes":[0,1,2]}],)"
			R"("nodes":[{"mesh":0},{"mesh":1},{"mesh":2}],)"
			R"("meshes":[)";
		for (int i = 0; i < 3; ++i)
			json += std::format(R"({}{{"primitives":[{{"attributes":{{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2}},"indices":3,"material":{}}}]}})", i ? "," : "", i);
		json += R"(],"materials":[)"
			R"({"name":"A","pbrMetallicRoughness":{"baseColorTexture":{"index":0},"baseColorFactor":[0.5,0.6,0.7,1.0],"metallicRoughnessTexture":{"index":2},"metallicFactor":0.8,"roughnessFactor":0.4},)"
			R"("normalTexture":{"index":1,"scale":0.5},"occlusionTexture":{"index":2,"strength":0.7},"emissiveTexture":{"index":3},"emissiveFactor":[1.0,0.5,0.25],)"
			R"("alphaMode":"BLEND","doubleSided":true,"extensions":{"KHR_materials_emissive_strength":{"emissiveStrength":2.0}}},)"
			R"({"name":"B","pbrMetallicRoughness":{"baseColorTexture":{"index":0}}},)"
			R"({"name":"C","pbrMetallicRoughness":{"baseColorFactor":[0.2,0.8,0.2,1.0],"metallicFactor":0.0,"roughnessFactor":0.5},"alphaMode":"MASK","alphaCutoff":0.5}],)"
			R"("textures":[{"source":0},{"source":1},{"source":2},{"source":3}],"images":[)";
		for (int i = 0; i < 4; ++i)
			json += std::format(R"({}{{"mimeType":"image/png","uri":"{}"}})", i ? "," : "", images[i]);
		json += std::format(R"(],"buffers":[{{"byteLength":140,"uri":"{}"}}],)", QuadBufferUri())
			+ R"("bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":48,"target":34962},{"buffer":0,"byteOffset":48,"byteLength":48,"target":34962},)"
			R"({"buffer":0,"byteOffset":96,"byteLength":32,"target":34962},{"buffer":0,"byteOffset":128,"byteLength":12,"target":34963}],)"
			R"("accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3","min":[-0.5,-0.5,0.0],"max":[0.5,0.5,0.0]},)"
			R"({"bufferView":1,"componentType":5126,"count":4,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":4,"type":"VEC2"},)"
			R"({"bufferView":3,"componentType":5123,"count":6,"type":"SCALAR"}]})";

		const std::filesystem::path file = folder / "import.gltf";
		{
			std::ofstream out(file, std::ios::binary);
			out << json;
		}

		m_ImportPbr = Model::LoadFromFile(file, ModelLoadParams().SetPbrMaterials(true));
		m_ImportPlain = Model::LoadFromFile(file);
		const bool loaded = m_ImportPbr && m_ImportPbr->GetSubMeshCount() == 3 && m_ImportPlain && m_ImportPlain->GetSubMeshCount() == 3;
		Check(loaded, "PBR 10: the test glTF loads with and without PbrMaterials, three submeshes each");
		if (!loaded)
			return;

		const std::vector<SubMesh>& submeshes = m_ImportPbr->GetSubMeshes();
		Material* a = submeshes[0].Material;
		Material* b = submeshes[1].Material;
		Material* c = submeshes[2].Material;
		Check(a && b && c && a->GetShader() && a->GetShader() == b->GetShader() && a->GetShader() == c->GetShader()
			&& a->GetShader() != m_ImportPlain->GetSubMeshes()[0].Material->GetShader(),
			"PBR 10: with the flag every material has the PBR shader, without it none does");
		if (!a || !b || !c)
			return;

		Check(Near(a->GetBaseColor(), glm::vec4(0.5f, 0.6f, 0.7f, 1.0f)) && Near(a->GetMetallic(), 0.8f) && Near(a->GetRoughness(), 0.4f)
			&& Near(a->GetOcclusion(), 0.7f) && Near(a->GetNormalScale(), 0.5f),
			std::format("PBR 10: material A has its base colour {} metallic {:.2f} roughness {:.2f} occlusion strength {:.2f} normal scale {:.2f} (file: 0.5 0.6 0.7, 0.8, 0.4, 0.7, 0.5)",
				Rgb(a->GetBaseColor()), a->GetMetallic(), a->GetRoughness(), a->GetOcclusion(), a->GetNormalScale()));
		Check(Near(a->GetEmissiveColor(), glm::vec3(1.0f, 0.5f, 0.25f)) && Near(a->GetEmissiveStrength(), 2.0f),
			std::format("PBR 10: material A's emissive colour is {} times {:.2f} (file: 1 0.5 0.25 times 2)", Rgb(a->GetEmissiveColor()), a->GetEmissiveStrength()));
		Check(a->IsTranslucent() && a->GetParams().CullMode == CullMode::None,
			std::format("PBR 10: material A, glTF blend and doubleSided, is Translucent ({}) with culling off ({})", a->IsTranslucent(), a->GetParams().CullMode == CullMode::None));
		Check(!b->IsTranslucent() && b->GetParams().CullMode == CullMode::Back && Near(b->GetMetallic(), 1.0f) && Near(b->GetRoughness(), 1.0f),
			std::format("PBR 10: material B, with no factors, is opaque ({}), culls back faces ({}) and has glTF's metallic {:.2f} and roughness {:.2f} of 1",
				!b->IsTranslucent(), b->GetParams().CullMode == CullMode::Back, b->GetMetallic(), b->GetRoughness()));
		Check(!c->IsTranslucent() && Near(c->GetBaseColor(), glm::vec4(0.2f, 0.8f, 0.2f, 1.0f)) && Near(c->GetMetallic(), 0.0f) && Near(c->GetRoughness(), 0.5f),
			"PBR 10: material C, glTF mask (warns once: read the log), draws opaque with its own factors");

		Check(a->GetTexture(2) && a->GetTexture(2) == a->GetTexture(3) && a->GetTexture(2) != a->GetTexture(0),
			"PBR 10: the image shared by A's metallic-roughness and occlusion slots loads once");
		Check(a->GetTexture(0) && a->GetTexture(0) == b->GetTexture(0) && a->GetTexture(0) == submeshes[0].DiffuseTexture
			&& a->GetTexture(1) && a->GetTexture(1) != b->GetTexture(1) && a->GetTexture(4) && a->GetTexture(4) != b->GetTexture(4),
			"PBR 10: the albedo image A and B share is one texture, DiffuseTexture is it, and B's empty slots hold a default, not A's images");

		bool mipped = true;
		for (uint32_t slot = 0; slot < Renderer3D::k_PbrTextureSlots; ++slot)
		{
			const Texture* texture = a->GetTexture(slot);
			mipped = mipped && texture && texture->GetParams().MipLevels == GetMipLevelCount(k_ImportImageSize, k_ImportImageSize);
		}
		const Texture* plainDiffuse = m_ImportPlain->GetSubMeshes()[0].DiffuseTexture;
		Check(mipped && plainDiffuse && plainDiffuse->GetParams().MipLevels == 1 && !m_ImportPlain->GetSubMeshes()[0].Material->GetTexture(1),
			"PBR 10: with the flag A's five images have full mip chains, without it the diffuse image has one mip and no other slot");

		struct MipCase
		{
			const char* Name;
			uint32_t Slot;
			Texel Left;
			Texel Right;
			Texel Average;
			int AverageTolerance;
		};
		const MipCase cases[4] = {
			{ "albedo", 0, k_AlbedoLeft, k_AlbedoRight, { 149, 73, 36, 255 }, 1 },
			{ "normal", 1, k_NormalLeft, k_NormalRight, { 128, 128, 255, 255 }, 3 },
			{ "metallic-roughness and occlusion", 2, k_OrmLeft, k_OrmRight, { 255, 150, 150, 255 }, 1 },
			{ "emissive", 4, k_EmissiveLeft, k_EmissiveRight, { 188, 128, 188, 255 }, 1 },
		};
		const uint32_t lastMip = GetMipLevelCount(k_ImportImageSize, k_ImportImageSize) - 1;
		const std::weak_ptr<int> alive = m_Alive;
		for (const MipCase& mip : cases)
		{
			Texture* texture = a->GetTexture(mip.Slot);
			if (!texture)
				continue;
			texture->ReadSubresource(1, 0, [this, alive, mip](const TexturePixels& pixels)
			{
				if (alive.expired())
					return;
				Check(TexelNear(pixels, 0, mip.Left, 1) && TexelNear(pixels, pixels.Width - 1, mip.Right, 1),
					std::format("PBR 10: the {} image's 4x4 mip keeps its halves (read {} and {})", mip.Name, TexelText(pixels, 0), TexelText(pixels, pixels.Width - 1)));
			});
			texture->ReadSubresource(lastMip, 0, [this, alive, mip](const TexturePixels& pixels)
			{
				if (alive.expired())
					return;
				Check(TexelNear(pixels, 0, mip.Average, mip.AverageTolerance),
					std::format("PBR 10: the {} image's 1x1 mip is the {} (read {}, expected {} {} {} {})", mip.Name,
						mip.Slot == 1 ? "renormalised average of its normals" : "average of its halves", TexelText(pixels, 0),
						mip.Average[0], mip.Average[1], mip.Average[2], mip.Average[3]));
			});
		}

		Framebuffer* target = CreateTarget(48, 16);
		const Renderer3D::Statistics stats = Render(target, glm::ortho(-1.5f, 1.5f, -0.5f, 0.5f, -1.0f, 1.0f), k_Black, [&](Renderer3D& renderer)
		{
			LightFromCamera(renderer, glm::vec3(1.0f), 1.0f, glm::vec3(0.0f));
			for (uint32_t i = 0; i < 3; ++i)
				renderer.SubmitMesh(submeshes[i].Mesh, glm::translate(glm::mat4(1.0f), glm::vec3(static_cast<float>(i) - 1.0f, 0.0f, 0.0f)), glm::vec4(1.0f), submeshes[i].Material);
		});
		Check(stats.PbrDraws == 3, std::format("PBR 10: Renderer3D draws the three imported materials as PBR ({} PBR draws)", stats.PbrDraws));
		ReadAll({ target }, [this](const std::vector<TexturePixels>& read)
		{
			const TexturePixels& image = read[0];
			const glm::vec3 first = PixelAt(image, 8, 8);
			const glm::vec3 second = PixelAt(image, 24, 8);
			const glm::vec3 third = PixelAt(image, 40, 8);
			Check((std::max)({ first.r, first.g, first.b }) > 0.02f && (std::max)({ second.r, second.g, second.b }) > 0.02f && third.g > third.r && third.g > third.b && third.g > 0.02f,
				std::format("PBR 10: every imported quad lights up, the green one green (read {}, {}, {})", Rgb(first), Rgb(second), Rgb(third)));
		});

		Material* const before[3] = { a, b, c };
		Texture* const albedoBefore = a->GetTexture(0);
		const uint32_t generation = m_ImportPbr->GetGeneration();
		const bool reloaded = m_ImportPbr->Reload();
		const std::vector<SubMesh>& after = m_ImportPbr->GetSubMeshes();
		Check(reloaded && m_ImportPbr->GetGeneration() == generation + 1 && after.size() == 3 && after[0].Material == before[0] && after[1].Material == before[1]
			&& after[2].Material == before[2] && before[0]->GetTexture(0) == albedoBefore && before[0]->GetTexture(0) && before[0]->GetTexture(3) == before[0]->GetTexture(2)
			&& Near(before[0]->GetMetallic(), 0.8f) && before[0]->GetParams().CullMode == CullMode::None,
			"PBR 10: a Reload keeps each PBR material and its textures and settings");
	}

	void PbrMaterialTest::CheckSceneSkinned()
	{
		m_SceneFoxPbr = Model::LoadFromFile("models/Fox/Fox.gltf", ModelLoadParams().SetPbrMaterials(true));
		m_SceneFoxPlain = Model::LoadFromFile("models/Fox/Fox.gltf");
		if (!m_Fox || !m_SceneFoxPbr || !m_SceneFoxPlain || !m_SceneFoxPbr->IsSkinned())
		{
			Check(false, "PBR 11: the Fox loads with and without PbrMaterials");
			return;
		}

		m_Scene = new Scene("PBR Test: skinned scene");
		Entity pbr = m_Scene->CreateEntity("Fox (PBR import)");
		pbr.AddComponent<Transform3DComponent>();
		pbr.AddComponent<SkinnedMeshRendererComponent>(SkinnedMeshRendererComponent(m_SceneFoxPbr));
		Entity plain = m_Scene->CreateEntity("Fox (plain import)");
		plain.AddComponent<Transform3DComponent>();
		plain.AddComponent<SkinnedMeshRendererComponent>(SkinnedMeshRendererComponent(m_SceneFoxPlain)).Visible = false;

		const glm::mat4 viewProjection = glm::ortho(-1.2f, 1.2f, -1.2f, 1.2f, -5.0f, 5.0f) * m_FoxTransform;
		auto draw = [&](Framebuffer* target)
		{
			return Render(target, viewProjection, k_Backdrop, [&](Renderer3D& renderer)
			{
				LightFromCamera(renderer, glm::vec3(1.0f), 0.8f, glm::vec3(0.1f));
				m_Scene->RenderEntities3D(renderer);
			});
		};

		Framebuffer* pbrTarget = CreateTarget(128, 128);
		const Renderer3D::Statistics pbrStats = draw(pbrTarget);

		pbr.GetComponent<SkinnedMeshRendererComponent>().Visible = false;
		plain.GetComponent<SkinnedMeshRendererComponent>().Visible = true;
		Framebuffer* plainTarget = CreateTarget(128, 128);
		const Renderer3D::Statistics plainStats = draw(plainTarget);

		Check(pbrStats.PbrDraws > 0 && pbrStats.SkinnedDraws > 0 && plainStats.PbrDraws == 0 && plainStats.SkinnedDraws > 0,
			std::format("PBR 11: a SkinnedMeshRendererComponent with a null Material draws a PBR-imported model's own PBR materials and a plain one's lit ({} PBR and {} skinned draws; plain {} and {})",
				pbrStats.PbrDraws, pbrStats.SkinnedDraws, plainStats.PbrDraws, plainStats.SkinnedDraws));

		ReadAll({ pbrTarget, plainTarget }, [this](const std::vector<TexturePixels>& images)
		{
			uint32_t covered = 0;
			for (uint32_t y = 0; y < images[0].Height; ++y)
			{
				for (uint32_t x = 0; x < images[0].Width; ++x)
					covered += LargestDelta(PixelAt(images[0], x, y), glm::vec3(k_Backdrop)) > 8.0f / 255.0f ? 1 : 0;
			}
			float worst = 0.0f;
			const uint32_t differing = CountDiffering(images[0], images[1], 2.0f / 255.0f, worst);
			Check(covered > 500 && differing > 100 && differing != ~0u,
				std::format("PBR 11: the scene's PBR Fox is on screen ({} px) and is not the lit picture ({} px differ)", covered, differing));
		});
	}

	void PbrMaterialTest::DrawView(float aspect)
	{
		const glm::mat4 viewProjection = glm::ortho(-k_ViewHalfHeight * aspect, k_ViewHalfHeight * aspect, -k_ViewHalfHeight, k_ViewHalfHeight, -5.0f, 5.0f);
		m_Renderer->BeginScene(viewProjection);
		m_Renderer->Clear(m_ClearColor);
		LightFromCamera(*m_Renderer, glm::vec3(1.0f), 0.9f, glm::vec3(0.03f));
		for (uint32_t row = 0; row < k_GridCells; ++row)
		{
			for (uint32_t column = 0; column < k_GridCells; ++column)
			{
				const glm::mat4 cell = glm::translate(glm::mat4(1.0f), { k_GridCentreX, 0.0f, 0.0f })
					* glm::scale(glm::mat4(1.0f), glm::vec3(k_GridScale))
					* CellAt(k_GridCells, k_GridCells, column, row);
				m_Renderer->SubmitMesh(m_ExactSphere, cell, glm::vec4(1.0f), m_GridMaterials[row * k_GridCells + column]);
			}
		}
		m_Renderer->EndScene();

		m_Renderer->BeginScene(viewProjection);
		LightFrom(*m_Renderer, { -1.0f, 0.8f, 0.8f }, 1.0f);
		m_Renderer->SetAmbientLight(glm::vec3(0.04f), 1.0f);
		for (uint32_t i = 0; i < 3; ++i)
		{
			const glm::mat4 transform = glm::translate(glm::mat4(1.0f), { k_PanelCentreX + (static_cast<float>(i) - 1.0f) * k_PanelSpacing, 0.0f, 0.0f })
				* glm::scale(glm::mat4(1.0f), glm::vec3(k_PanelScale));
			m_Renderer->SubmitMesh(m_ExactSphere, transform, glm::vec4(1.0f), m_PanelMaterials[i]);
		}
		m_Renderer->EndScene();
	}

	void PbrMaterialTest::DrawLabels(float aspect)
	{
		if (!m_Font)
			return;

		Renderer2D& renderer2D = Application::Get().GetRenderer2D();
		renderer2D.BeginScene(glm::ortho(-k_ViewHalfHeight * aspect, k_ViewHalfHeight * aspect, -k_ViewHalfHeight, k_ViewHalfHeight, -1.0f, 1.0f));
		Renderer2D::TextParameters centered;
		centered.Centered = true;
		const float half = 0.5f * static_cast<float>(k_GridCells - 1);
		for (uint32_t i = 0; i < k_GridCells; ++i)
		{
			const float x = k_GridCentreX + (static_cast<float>(i) - half) * k_GridScale;
			const float y = (half - static_cast<float>(i)) * k_GridScale;
			renderer2D.DrawText(std::format("{:.2f}", k_Roughness[i]), m_Font, glm::vec2(x, -k_GridScale * 2.85f), 0.25f, centered);
			renderer2D.DrawText(std::format("{:.2f}", k_Metallic[i]), m_Font, glm::vec2(k_GridCentreX - k_GridScale * 3.2f, y - 0.09f), 0.25f, centered);
		}
		renderer2D.DrawText("Roughness (columns)", m_Font, glm::vec2(k_GridCentreX, -k_GridScale * 3.3f), 0.3f, centered);
		Renderer2D::TextParameters vertical = centered;
		vertical.Rotation = 90.0f;
		renderer2D.DrawText("Metallic (rows)", m_Font, glm::vec2(k_GridCentreX - k_GridScale * 3.9f, 0.0f), 0.3f, vertical);
		renderer2D.DrawText("PBR grid, light from the camera", m_Font, glm::vec2(k_GridCentreX, k_GridScale * 3.0f), 0.3f, centered);

		const char* const names[3] = { "flat", "normal map", "scale 2" };
		for (uint32_t i = 0; i < 3; ++i)
			renderer2D.DrawText(names[i], m_Font, glm::vec2(k_PanelCentreX + (static_cast<float>(i) - 1.0f) * k_PanelSpacing, -1.2f), 0.24f, centered);
		renderer2D.DrawText("Normal map, light upper left", m_Font, glm::vec2(k_PanelCentreX, k_GridScale * 3.0f), 0.26f, centered);
		renderer2D.EndScene();
	}

	void PbrMaterialTest::Update(float deltaTime)
	{
		Renderer::Clear(m_ClearColor);
		DrawView(m_AspectRatio);
		DrawLabels(m_AspectRatio);

		if (Renderer::IsFrameSkipped())
			return;
		if (m_NextStep < m_Steps.size())
			m_Steps[m_NextStep++]();
	}

	void PbrMaterialTest::Cleanup()
	{
		m_Alive.reset();
		m_Steps.clear();
		m_StaticBatch.reset();
		for (Framebuffer*& target : m_Targets)
			DestroyAndDelete(target);
		m_Targets.clear();
		for (Material*& material : m_Materials)
			DestroyAndDelete(material);
		m_Materials.clear();
		m_GridMaterials.clear();
		for (Material*& material : m_PanelMaterials)
			material = nullptr;
		delete m_RepeatSampler;
		m_RepeatSampler = nullptr;
		m_FoxMaterial = nullptr;
		delete m_Scene;
		m_Scene = nullptr;
		delete m_SceneFoxPbr;
		delete m_SceneFoxPlain;
		m_SceneFoxPbr = nullptr;
		m_SceneFoxPlain = nullptr;
		delete m_ImportPbr;
		delete m_ImportPlain;
		m_ImportPbr = nullptr;
		m_ImportPlain = nullptr;
		if (!m_ImportFolder.empty())
		{
			std::error_code ignored;
			std::filesystem::remove_all(m_ImportFolder, ignored);
			m_ImportFolder.clear();
		}
		delete m_Fox;
		m_Fox = nullptr;
		delete m_ExactSphere;
		delete m_SmallSphere;
		delete m_Quad;
		m_ExactSphere = nullptr;
		m_SmallSphere = nullptr;
		m_Quad = nullptr;
		DestroyAndDelete(m_HalfAlpha);
		for (Texture*& texture : m_Textures)
			DestroyAndDelete(texture);
		m_Textures.clear();
		for (Mesh*& mesh : m_Meshes)
			delete mesh;
		m_Meshes.clear();
		DestroyAndDelete(m_Font);
		if (m_Renderer)
		{
			m_Renderer->Shutdown();
			delete m_Renderer;
			m_Renderer = nullptr;
		}
	}

	void PbrMaterialTest::Resize(uint32_t width, uint32_t height)
	{
		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
	}

	void PbrMaterialTest::ImGuiRender()
	{
		GraphicsTest::ImGuiRender();
		ImGui::Separator();
		for (const CheckResult& check : m_Checks)
		{
			ImGui::TextColored(check.Passed ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(0.9f, 0.3f, 0.3f, 1.0f),
				"[%s] %s", check.Passed ? "PASS" : "FAIL", check.Name.c_str());
		}
	}

}
