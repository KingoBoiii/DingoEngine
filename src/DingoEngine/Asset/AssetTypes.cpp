#include "depch.h"
#include "DingoEngine/Asset/AssetTypes.h"

#include <algorithm>
#include <cctype>

namespace Dingo
{

	const char* AssetTypeToString(AssetType type)
	{
		switch (type)
		{
			case AssetType::None:      return "None";
			case AssetType::Texture2D: return "Texture2D";
			case AssetType::Shader:    return "Shader";
			case AssetType::Model:     return "Model";
			case AssetType::Font:      return "Font";
			case AssetType::AudioClip: return "AudioClip";
		}
		return "Unknown";
	}

	AssetType AssetTypeFromExtension(const std::filesystem::path& extension)
	{
		std::string ext = extension.string();
		std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp")
			return AssetType::Texture2D;
		if (ext == ".glsl" || ext == ".shader")
			return AssetType::Shader;
		if (ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".fbx")
			return AssetType::Model;
		if (ext == ".ttf" || ext == ".otf")
			return AssetType::Font;
		if (ext == ".wav" || ext == ".ogg" || ext == ".mp3")
			return AssetType::AudioClip;

		return AssetType::None;
	}

}
