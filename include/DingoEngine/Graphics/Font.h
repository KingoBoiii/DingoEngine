#pragma once

#include "DingoEngine/Graphics/Texture.h"

namespace Dingo
{

	struct MSDFData;

	struct BoundingBox
	{
		glm::vec2 QuadMin;
		glm::vec2 QuadMax;
	};

	struct FontParams
	{
		std::string Name;
		uint8_t ThreadCount = 8;
		bool UseExpensiveEdgeColoring = false;
	};

	class Font
	{
	public:
		static Font* Create(const std::filesystem::path& filepath, const FontParams& params = {});

	public:
		Font(const std::filesystem::path& filepath, const FontParams& params);
		~Font() = default;

	public:
		void Initialize();
		void Destroy();

		float GetStringWidth(const std::string& string, float size = 1.0f) const;
		BoundingBox GetBoundingBox(const std::string& string, float size = 1.0f) const;
		Texture* GetAtlasTexture() const { return m_AtlasTexture; }
		const MSDFData* GetMSDFData() const { return m_Data; }

	private:
		void InitializeFontData(int32_t& width, int32_t& height);

	private:
		FontParams m_Params;
		std::string m_Name;
		std::filesystem::path m_FilePath;
		Texture* m_AtlasTexture = nullptr;
		MSDFData* m_Data = nullptr;
	};

}


