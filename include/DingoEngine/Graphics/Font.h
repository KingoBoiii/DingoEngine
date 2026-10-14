#pragma once

#include "DingoEngine/Graphics/Texture.h"

namespace Dingo
{

	struct MSDFData;

	namespace Internal::TextLayout
	{
		struct FontGlyphSource;
	}

	/// The size of a block of text, from Font::MeasureText.
	struct TextMetrics
	{
		float Width = 0.0f;			///< Pen width of the widest line, as GetStringWidth reports it.
		/// From the first line's ascender to the last line's descender: one size per line plus the
		/// gaps between baselines (GetLineHeight) for every line after the first.
		float Height = 0.0f;
		uint32_t LineCount = 0;		///< Lines after '\n' and wrap breaks; 0 for an empty string.
	};

	/// Options for Font::Create.
	struct FontParams
	{
		/// Name used in logs and for the atlas cache file; empty = the file's name.
		std::string Name;
		uint8_t ThreadCount = 8; ///< Threads used to generate the atlas when it isn't cached.
		/// Slower atlas generation with better corners; part of the atlas cache key.
		bool UseExpensiveEdgeColoring = false;
	};

	/// A font file baked into a multi-channel signed-distance-field atlas for Renderer2D::DrawText.
	/// The caller owns it; delete frees the atlas. The atlas is cached on disk after the first run.
	class Font
	{
	public:
		/// Returns nullptr on failure (error is logged). Caller owns the returned Font.
		/// A relative filepath is looked up under the asset root first, then the working
		/// directory.
		[[nodiscard]] static Font* Create(const std::filesystem::path& filepath, const FontParams& params = {});

	public:
		/// Frees the atlas texture and the glyph data.
		/// Defined in Font.cpp (where MSDFData is complete) so m_Data is deleted correctly.
		~Font();
		/// Fonts are not copyable.
		Font(const Font&) = delete;
		/// Fonts are not copyable.
		Font& operator=(const Font&) = delete;

	public:
		/// Releases the atlas early; the destructor releases everything anyway.
		void Destroy();

		/// True if the font loaded successfully and has a usable atlas + glyph geometry.
		bool IsValid() const;

		/// Width of the widest line, in the same units Renderer2D::DrawText lays glyphs out in.
		/// `kerning` must match TextParameters::Kerning, which the pen adds per character -
		/// leave it at 0 and the two disagree by one Kerning per character.
		float GetStringWidth(const std::string& string, float size = 1.0f, float kerning = 0.0f) const;
		/// Distance from one baseline to the next at `size`, in the units GetStringWidth returns.
		/// `lineSpacing` is TextParameters::LineSpacing (units of the size), added to the font's own.
		float GetLineHeight(float size = 1.0f, float lineSpacing = 0.0f) const;
		/// Height of the ascender above the baseline at `size`; ascender minus descender is `size`.
		float GetAscender(float size = 1.0f) const;
		/// Height of the descender relative to the baseline at `size`: negative, below it.
		float GetDescender(float size = 1.0f) const;
		/// Measures `string` as Renderer2D::DrawText lays it out with the same size, Kerning,
		/// LineSpacing and MaxWidth. maxWidth > 0 wraps at spaces, and breaks a word wider than
		/// the line between characters; a line always takes at least one character, so Width
		/// exceeds maxWidth only when one character alone is wider. Zero for an invalid font.
		TextMetrics MeasureText(const std::string& string, float size = 1.0f, float maxWidth = 0.0f, float kerning = 0.0f, float lineSpacing = 0.0f) const;
		/// The atlas texture the font draws from; owned by the font, so it dies with the font.
		Texture* GetAtlasTexture() const { return m_AtlasTexture; }

	private:
		Font(const std::filesystem::path& filepath, const FontParams& params);
		bool Initialize();
		const MSDFData* GetMSDFData() const { return m_Data; }
		friend class Renderer2D;
		friend struct Internal::TextLayout::FontGlyphSource;

		// `fontData` is borrowed by FreeType for the duration of the call, so the caller's
		// buffer must outlive it.
		bool InitializeFontData(const uint8_t* fontData, size_t fontDataSize, int32_t& width, int32_t& height);

	private:
		FontParams m_Params;
		std::string m_Name;
		std::filesystem::path m_FilePath;
		Texture* m_AtlasTexture = nullptr;
		MSDFData* m_Data = nullptr;
	};

}


