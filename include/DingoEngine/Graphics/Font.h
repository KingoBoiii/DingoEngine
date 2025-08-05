#pragma once

#include "DingoEngine/Graphics/Texture.h"

namespace Dingo
{

	struct MSDFData;

	class Font
	{
	public:
		static Font* Create(const std::filesystem::path& filepath);

	public:
		Font(const std::filesystem::path& filepath);
		~Font() = default;

	public:
		void Initialize();
		void Destroy();

		Texture* GetAtlasTexture() const { return m_AtlasTexture; }
		const MSDFData* GetMSDFData() const { return m_Data; }

	private:
		std::filesystem::path m_FilePath;
		Texture* m_AtlasTexture = nullptr;
		MSDFData* m_Data = nullptr;
	};

}


