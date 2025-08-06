#include "depch.h"
#include "DingoEngine/Graphics/Font.h"
#include "DingoEngine/Core/CacheManager.h"

#undef INFINITE
#define MSDFGEN_PUBLIC
#include <msdf-atlas-gen.h>
#include <FontGeometry.h>

#include "MSDFData.h"

namespace Dingo
{

	struct FontAtlasHeader
	{
		uint32_t Version = DE_MAKE_VERSION(1, 0, 0);
		uint32_t Width = 0;
		uint32_t Height = 0;
	};

	namespace Utils
	{

		inline static std::filesystem::path GetCacheFilePath(const std::string& name, float fontSize)
		{
			std::filesystem::path cachePath = CacheManager::GetCacheDirectory("fonts\\atlas");
			std::string filename = std::format("{0}-{1}.dfa", name, fontSize);
			return cachePath / filename;
		}

		inline static void CacheFontAtlas(const std::string& name, float fontSize, FontAtlasHeader& header, const void* pixels)
		{
			std::filesystem::path filePath = GetCacheFilePath(name, fontSize);

			std::ofstream stream(filePath, std::ios::binary | std::ios::trunc);
			if (!stream)
			{
				stream.close();
				DE_CORE_ERROR_TAG("Font", "Failed to cache font atlas to {}", filePath.string());
				return;
			}

			stream.write((const char*)&header, sizeof(FontAtlasHeader));
			stream.write((const char*)pixels, static_cast<uint64_t>(header.Width) * header.Height * 4); // Assuming RGBA8 format

			stream.close();
		}

		template<typename T, typename S, int N, msdf_atlas::GeneratorFunction<S, N> GenFunc>
		inline static Texture* CreateAndCacheAtlas(const std::string& fontName, float fontSize, const std::vector<msdf_atlas::GlyphGeometry>& glyphs, const msdf_atlas::FontGeometry& fontGeometry, uint32_t width, uint32_t height, const FontParams& params)
		{
			msdf_atlas::GeneratorAttributes attributes;
			attributes.config.overlapSupport = true;
			attributes.scanlinePass = true;

			msdf_atlas::ImmediateAtlasGenerator<S, N, GenFunc, msdf_atlas::BitmapAtlasStorage<T, N>> generator(width, height);
			generator.setAttributes(attributes);
			generator.setThreadCount(params.ThreadCount);
			generator.generate(glyphs.data(), (int)glyphs.size());

			msdfgen::BitmapConstRef<T, N> bitmap = (msdfgen::BitmapConstRef<T, N>)generator.atlasStorage();

			FontAtlasHeader header;
			header.Width = width;
			header.Height = height;
			CacheFontAtlas(fontName, fontSize, header, bitmap.pixels);

			Texture* texture = Texture::CreateFromData(width, height, (void*)bitmap.pixels, TextureFormat::RGBA8_UNORM);
			return texture;
		}

	}

	Font* Font::Create(const std::filesystem::path& filepath, const FontParams& params)
	{
		Font* font = new Font(filepath, params);
		font->Initialize();
		return font;
	}

	Font::Font(const std::filesystem::path& filepath, const FontParams& params)
		: m_Params(params), m_FilePath(filepath), m_Data(new MSDFData())
	{}

	void Font::Initialize()
	{
		int32_t width, height;
		InitializeFontData(width, height);

		m_Name = m_Params.Name.empty() ? m_FilePath.stem().string() : m_Params.Name;

		// first check if the font atlas is cached
		std::filesystem::path filePath = Utils::GetCacheFilePath(m_Name, 0.0f);
		if (std::filesystem::exists(filePath))
		{
			DE_CORE_INFO("Loading cached font atlas from {}", filePath.string());
			std::ifstream stream(filePath, std::ios::binary);
			if (!stream)
			{
				stream.close();
				DE_CORE_ERROR_TAG("Font", "Failed to open cached font atlas file: {}", filePath.string());
				return;
			}

			FontAtlasHeader header;
			stream.read((char*)&header, sizeof(FontAtlasHeader));
			if (stream.gcount() != sizeof(FontAtlasHeader))
			{
				stream.close();
				DE_CORE_ERROR_TAG("Font", "Failed to read font atlas header from file: {}", filePath.string());
				return;
			}

			// read texture pixels from cache file
			uint64_t pixelDataSize = static_cast<uint64_t>(header.Width) * header.Height * 4; // Assuming RGBA8 format
			std::vector<uint8_t> pixelData(pixelDataSize);
			stream.read((char*)pixelData.data(), pixelDataSize);
			//if (stream.gcount() != pixelDataSize)
			//{
			//	stream.close();
			//	DE_CORE_ERROR_TAG("Font", "Failed to read pixel data from cached font atlas file: {}", filePath.string());
			//	return;
			//}

			stream.close();

			m_AtlasTexture = Texture::CreateFromData(header.Width, header.Height, (void*)(pixelData.data()), TextureFormat::RGBA8_UNORM);
		}
		else
		{
			m_AtlasTexture = Utils::CreateAndCacheAtlas<uint8_t, float, 4, msdf_atlas::mtsdfGenerator>(m_Name, 0.0f, m_Data->Glyphs, m_Data->FontGeometry, width, height, m_Params);
		}
	}

	void Font::Destroy()
	{
		if (m_AtlasTexture)
		{
			m_AtlasTexture->Destroy();
			m_AtlasTexture = nullptr;
		}
	}

	void Font::InitializeFontData(int32_t& width, int32_t& height)
	{
		msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
		DE_CORE_ASSERT(ft, "Failed to initialize FreeType");

		std::string fileString = m_FilePath.string();
		msdfgen::FontHandle* font = msdfgen::loadFont(ft, fileString.c_str());
		if (!font)
		{
			DE_CORE_ERROR("Failed to load font: {}", fileString);
			return;
		}

		struct CharsetRange
		{
			uint32_t Begin, End;
		};

		// From imgui_draw.cpp
		static const CharsetRange charsetRange[] = {
			{ 0x0020, 0x00FF }
		};

		msdf_atlas::Charset charset;
		for (CharsetRange range : charsetRange)
		{
			for (uint32_t c = range.Begin; c <= range.End; c++)
			{
				charset.add(c);
			}
		}

		double fontScale = 1.0;
		m_Data->FontGeometry = msdf_atlas::FontGeometry(&m_Data->Glyphs);
		int glyphsLoaded = m_Data->FontGeometry.loadCharset(font, fontScale, charset);
		DE_CORE_INFO("Loading {} glyphs from font (out of {})", glyphsLoaded, charset.size());

		double emSize = 40.0;

		msdf_atlas::TightAtlasPacker atlasPacker;
		//atlasPacker.setDimensionsConstraint(msdf_atlas::TightAtlasPacker::DimensionsConstraint::EVEN_SQUARE);
		atlasPacker.setPixelRange(2.0);
		atlasPacker.setMiterLimit(1.0);
		//atlasPacker.setPadding(0.0);
		atlasPacker.setScale(emSize);
		int remaining = atlasPacker.pack(m_Data->Glyphs.data(), (int)m_Data->Glyphs.size());
		DE_CORE_ASSERT(remaining == 0);

		atlasPacker.getDimensions(width, height);
		emSize = atlasPacker.getScale();

#define DEFAULT_ANGLE_THRESHOLD 3.0
#define LCG_MULTIPLIER 6364136223846793005ull
#define LCG_INCREMENT 1442695040888963407ull

		// if MSDF || MTSDF 
		// Edge coloring
		uint64_t coloringSeed = 0;
		if (m_Params.UseExpensiveEdgeColoring)
		{
			msdf_atlas::Workload([&glyphs = m_Data->Glyphs, &coloringSeed](int i, int threadNo) -> bool
			{
				unsigned long long glyphSeed = (LCG_MULTIPLIER * (coloringSeed ^ i) + LCG_INCREMENT) * !!coloringSeed;
				glyphs[i].edgeColoring(msdfgen::edgeColoringInkTrap, DEFAULT_ANGLE_THRESHOLD, glyphSeed);
				return true;
			}, m_Data->Glyphs.size()).finish(m_Params.ThreadCount);
		}
		else
		{
			unsigned long long glyphSeed = coloringSeed;
			for (msdf_atlas::GlyphGeometry& glyph : m_Data->Glyphs)
			{
				glyphSeed *= LCG_MULTIPLIER;
				glyph.edgeColoring(msdfgen::edgeColoringInkTrap, DEFAULT_ANGLE_THRESHOLD, glyphSeed);
			}
		}

#if 0
		msdfgen::Shape shape;
		if (msdfgen::loadGlyph(shape, font, 'C'))
		{
			shape.normalize();

			msdfgen::edgeColoringSimple(shape, 3.0);

			msdfgen::Bitmap<float, 3> msdf(32, 32);

			msdfgen::generateMSDF(msdf, shape, 4.0, 1.0, msdfgen::Vector2(4.0, 4.0));

			msdfgen::savePng(msdf, "output.png");
		}
#endif

		msdfgen::destroyFont(font);

		msdfgen::deinitializeFreetype(ft);
	}

}
