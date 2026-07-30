#include "depch.h"
#include "DingoEngine/Graphics/Font.h"
#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Core/CacheManager.h"

#undef INFINITE
#define MSDFGEN_PUBLIC
#include <msdf-atlas-gen.h>
#include <FontGeometry.h>

#include "MSDFData.h"

namespace Dingo
{

	// Atlas cache files start with this header, binding the cached pixels to the exact font
	// file and generation parameters they came from: a .ttf replaced in place would
	// otherwise pair new glyph geometry with the old atlas — silently garbled text. Mirrors
	// the shader bytecode cache in NvrhiShader.cpp. Header-less files from older engine
	// versions fail the magic check and regenerate the same way.
	struct FontAtlasHeader
	{
		uint32_t Magic = 0;
		uint32_t FormatVersion = 0;
		uint64_t SourceHash = 0;
		uint32_t Width = 0;
		uint32_t Height = 0;
		// Size and content hash of the pixels that follow. Size alone leaves length-preserving
		// damage undetected, and nothing ever re-validates an atlas once it is on disk.
		uint64_t PayloadSize = 0;
		uint64_t PayloadHash = 0;
	};

	namespace Utils
	{

		static constexpr uint32_t k_FontAtlasCacheMagic = 0x46414344; // "DCAF"
		// Bump when the charset, the packer settings, the generator or this header change in a
		// way that invalidates previously cached pixels.
		static constexpr uint32_t k_FontAtlasCacheFormatVersion = 2;

		inline static uint64_t HashFNV1a(const void* data, size_t size, uint64_t hash = 14695981039346656037ull)
		{
			const uint8_t* bytes = static_cast<const uint8_t*>(data);
			for (size_t i = 0; i < size; i++)
			{
				hash ^= bytes[i];
				hash *= 1099511628211ull;
			}
			return hash;
		}

		inline static std::string SanitizeFileName(const std::string& name)
		{
			std::string sanitized = name;
			for (char& c : sanitized)
			{
				const bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_';
				if (!safe)
					c = '_';
			}
			return sanitized;
		}

		// The name alone is the file stem for most fonts, so two same-named fonts in
		// different directories would share one cache entry; the path hash separates them.
		inline static std::filesystem::path GetCacheFilePath(const std::string& name, const std::filesystem::path& fontPath)
		{
			std::filesystem::path cachePath = CacheManager::GetCacheDirectory("fonts\\atlas");
			const std::string pathKey = fontPath.generic_string();
			return cachePath / std::format("{}-{:016x}.dfa", SanitizeFileName(name), HashFNV1a(pathKey.data(), pathKey.size()));
		}

		inline static std::vector<uint8_t> ReadFileBytes(const std::filesystem::path& filePath)
		{
			std::error_code ec;
			const uintmax_t fileSize = std::filesystem::file_size(filePath, ec);
			if (ec)
				return {};

			std::ifstream stream(filePath, std::ios::binary);
			if (!stream)
				return {};

			std::vector<uint8_t> bytes(static_cast<size_t>(fileSize));
			stream.read((char*)bytes.data(), bytes.size());
			if (static_cast<uintmax_t>(stream.gcount()) != fileSize)
				return {};

			return bytes;
		}

		// Hash of the font file's bytes plus every parameter that changes the pixels the
		// generator produces.
		inline static uint64_t ComputeFontSourceHash(const std::vector<uint8_t>& fontData, const FontParams& params)
		{
			const uint64_t hash = HashFNV1a(fontData.data(), fontData.size());
			const uint8_t expensiveEdgeColoring = params.UseExpensiveEdgeColoring ? 1u : 0u;
			return HashFNV1a(&expensiveEdgeColoring, sizeof(expensiveEdgeColoring), hash);
		}

		// False when missing, unreadable, from an older format, generated from a different
		// font or with different parameters, or damaged — the caller regenerates, exactly as
		// if the file were absent. `width`/`height` are what the packer sized this run's
		// atlas to; a header naming anything else describes a different atlas.
		inline static bool ReadCachedAtlas(const std::filesystem::path& filePath, uint64_t sourceHash, uint32_t width, uint32_t height, std::vector<uint8_t>& outPixels)
		{
			std::error_code ec;
			const uintmax_t fileSize = std::filesystem::file_size(filePath, ec);
			if (ec || fileSize < sizeof(FontAtlasHeader))
				return false;

			std::ifstream stream(filePath, std::ios::binary);
			if (!stream)
				return false;

			FontAtlasHeader header;
			stream.read((char*)&header, sizeof(FontAtlasHeader));
			if (!stream || header.Magic != k_FontAtlasCacheMagic || header.FormatVersion != k_FontAtlasCacheFormatVersion || header.SourceHash != sourceHash)
				return false;

			if (header.Width != width || header.Height != height)
				return false;

			// Size the read from the header and demand the file agrees: a truncated cache
			// otherwise sizes its vector from a header that is still intact, and the missing
			// tail is uploaded as uninitialized pixels.
			const uint64_t expectedPayload = static_cast<uint64_t>(width) * height * 4; // RGBA8
			if (header.PayloadSize != expectedPayload || fileSize - sizeof(FontAtlasHeader) != header.PayloadSize)
				return false;

			outPixels.resize(static_cast<size_t>(header.PayloadSize));
			stream.read((char*)outPixels.data(), outPixels.size());
			if (static_cast<uint64_t>(stream.gcount()) != header.PayloadSize)
				return false;

			// What the checks above cannot see is damage that preserves the length — a write
			// interrupted after the file was extended, or bit rot. Nothing re-validates an
			// atlas once it is cached, so that garbling would be permanent.
			return HashFNV1a(outPixels.data(), outPixels.size()) == header.PayloadHash;
		}

		inline static void CacheFontAtlas(const std::filesystem::path& filePath, const FontAtlasHeader& header, const void* pixels)
		{
			std::ofstream stream(filePath, std::ios::binary | std::ios::trunc);
			if (!stream)
			{
				DE_CORE_ERROR_TAG("Font", "Failed to cache font atlas to {}", filePath.string());
				return;
			}

			stream.write((const char*)&header, sizeof(FontAtlasHeader));
			stream.write((const char*)pixels, header.PayloadSize);
		}

		template<typename T, typename S, int N, msdf_atlas::GeneratorFunction<S, N> GenFunc>
		inline static Texture* CreateAndCacheAtlas(const std::filesystem::path& cacheFilePath, uint64_t sourceHash, const std::vector<msdf_atlas::GlyphGeometry>& glyphs, uint32_t width, uint32_t height, const FontParams& params)
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
			header.Magic = k_FontAtlasCacheMagic;
			header.FormatVersion = k_FontAtlasCacheFormatVersion;
			header.SourceHash = sourceHash;
			header.Width = width;
			header.Height = height;
			header.PayloadSize = static_cast<uint64_t>(width) * height * 4;
			header.PayloadHash = HashFNV1a(bitmap.pixels, static_cast<size_t>(header.PayloadSize));
			CacheFontAtlas(cacheFilePath, header, bitmap.pixels);

			return Texture::CreateFromData(width, height, (void*)bitmap.pixels, TextureFormat::RGBA8_UNORM);
		}

	}

	Font* Font::Create(const std::filesystem::path& filepath, const FontParams& params)
	{
		Font* font = new Font(filepath, params);
		if (!font->Initialize())
		{
			delete font;
			return nullptr;
		}
		return font;
	}

	Font::Font(const std::filesystem::path& filepath, const FontParams& params)
		: m_Params(params), m_FilePath(filepath), m_Data(new MSDFData())
	{}

	Font::~Font()
	{
		// Route through Destroy() so the delete-without-Destroy path (e.g. Create() failure)
		// still releases m_Data and the atlas.
		Destroy();
	}

	bool Font::Initialize()
	{
		// One read of the file, shared by the source hash and FreeType. It has to outlive
		// InitializeFontData: FT_New_Memory_Face borrows the buffer rather than copying it,
		// and only releases it when that function destroys the font.
		const std::vector<uint8_t> fontData = Utils::ReadFileBytes(m_FilePath);
		if (fontData.empty())
		{
			DE_CORE_ERROR("Failed to read font file: {}", m_FilePath.string());
			return false;
		}

		int32_t width = 0, height = 0;
		if (!InitializeFontData(fontData.data(), fontData.size(), width, height))
		{
			// Font data failed to load (e.g. missing/invalid font file) - do not touch
			// width/height or proceed to atlas generation, both are meaningless here.
			return false;
		}

		m_Name = m_Params.Name.empty() ? m_FilePath.stem().string() : m_Params.Name;

		const std::filesystem::path cacheFilePath = Utils::GetCacheFilePath(m_Name, m_FilePath);
		const uint64_t sourceHash = Utils::ComputeFontSourceHash(fontData, m_Params);

		std::vector<uint8_t> pixelData;
		if (Utils::ReadCachedAtlas(cacheFilePath, sourceHash, static_cast<uint32_t>(width), static_cast<uint32_t>(height), pixelData))
		{
			DE_CORE_INFO("Loading cached font atlas from {}", cacheFilePath.string());
			m_AtlasTexture = Texture::CreateFromData(width, height, (void*)pixelData.data(), TextureFormat::RGBA8_UNORM);
		}
		else
		{
			m_AtlasTexture = Utils::CreateAndCacheAtlas<uint8_t, float, 4, msdf_atlas::mtsdfGenerator>(cacheFilePath, sourceHash, m_Data->Glyphs, width, height, m_Params);
		}

		return m_AtlasTexture != nullptr;
	}

	void Font::Destroy()
	{
		DestroyAndDelete(m_AtlasTexture);

		// Owned MSDFData; freed here (not just in ~Font) so it's released even when callers
		// only call Destroy() without deleting. Idempotent: delete nullptr + re-null is safe.
		delete m_Data;
		m_Data = nullptr;
	}

	float Font::GetStringWidth(const std::string& string, float size, float kerning) const
	{
		if (string.empty() || !IsValid())
		{
			return 0.0f;
		}

		const msdf_atlas::FontGeometry& fontGeometry = m_Data->FontGeometry;
		const msdfgen::FontMetrics& metrics = fontGeometry.getMetrics();
		const double fsScale = 1.0 / (metrics.ascenderY - metrics.descenderY);

		const msdf_atlas::GlyphGeometry* spaceGlyph = fontGeometry.getGlyph(' ');
		const double spaceAdvance = spaceGlyph ? spaceGlyph->getAdvance() : 0.0;

		double x = 0.0;
		double widestLine = 0.0;

		for (size_t i = 0; i < string.size(); i++)
		{
			const char character = string[i];

			if (character == '\n')
			{
				if (x > widestLine)
					widestLine = x;

				x = 0.0;
				continue;
			}

			if (character == '\t')
			{
				x += 4.0 * (fsScale * spaceAdvance + kerning);
				continue;
			}

			const msdf_atlas::GlyphGeometry* glyph = fontGeometry.getGlyph(character);
			if (!glyph)
			{
				glyph = fontGeometry.getGlyph('?');
			}
			if (!glyph)
			{
				continue;
			}

			// Pen advance, not ink width — a space has an advance but no plane bounds.
			// getAdvance() is an out-param that *replaces* the value, so seed it with the
			// glyph's own advance and only let the kerned pair overwrite it when there is
			// a next character.
			double advance = glyph->getAdvance();
			if (i + 1 < string.size())
			{
				fontGeometry.getAdvance(advance, character, string[i + 1]);
			}

			x += fsScale * advance + kerning;
		}

		if (x > widestLine)
			widestLine = x;

		return static_cast<float>(widestLine * size);
	}

	bool Font::InitializeFontData(const uint8_t* fontData, size_t fontDataSize, int32_t& width, int32_t& height)
	{
		msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
		DE_CORE_ASSERT(ft, "Failed to initialize FreeType");

		// The caller's buffer must stay alive until destroyFont below: FT_New_Memory_Face
		// borrows it.
		msdfgen::FontHandle* font = msdfgen::loadFontData(ft, reinterpret_cast<const msdfgen::byte*>(fontData), static_cast<int>(fontDataSize));
		if (!font)
		{
			DE_CORE_ERROR("Failed to load font: {}", m_FilePath.string());
			msdfgen::deinitializeFreetype(ft);
			return false;
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

		return true;
	}

	bool Font::IsValid() const
	{
		return m_AtlasTexture != nullptr && m_Data != nullptr && !m_Data->Glyphs.empty();
	}

}
