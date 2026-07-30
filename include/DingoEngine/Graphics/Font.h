#pragma once

#include "DingoEngine/Graphics/Texture.h"

namespace Dingo
{

	struct MSDFData;

	struct FontParams
	{
		std::string Name;
		uint8_t ThreadCount = 8;
		bool UseExpensiveEdgeColoring = false;
	};

	class Font
	{
	public:
		// Returns nullptr on failure (error is logged). Caller owns the returned Font.
		static Font* Create(const std::filesystem::path& filepath, const FontParams& params = {});

	public:
		Font(const std::filesystem::path& filepath, const FontParams& params);
		// Defined in Font.cpp (where MSDFData is complete) so m_Data is deleted correctly.
		~Font();

	public:
		bool Initialize();
		void Destroy();

		// True if the font loaded successfully and has a usable atlas + glyph geometry.
		bool IsValid() const;

		// Width of the widest line, in the same units Renderer2D::DrawText lays glyphs out in.
		// `kerning` must match TextParameters::Kerning, which the pen adds per character —
		// leave it at 0 and the two disagree by one Kerning per character.
		float GetStringWidth(const std::string& string, float size = 1.0f, float kerning = 0.0f) const;
		Texture* GetAtlasTexture() const { return m_AtlasTexture; }
		const MSDFData* GetMSDFData() const { return m_Data; }

	private:
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


