#pragma once

#include "DingoEngine/Graphics/Texture.h"

namespace Dingo
{

	struct MSDFData;

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
		/// The atlas texture the font draws from; owned by the font, so it dies with the font.
		Texture* GetAtlasTexture() const { return m_AtlasTexture; }

	private:
		Font(const std::filesystem::path& filepath, const FontParams& params);
		bool Initialize();
		const MSDFData* GetMSDFData() const { return m_Data; }
		friend class Renderer2D;

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


