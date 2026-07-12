#pragma once
#include "DingoEngine/Asset/AssetTypes.h"

#include <filesystem>

namespace Dingo
{

	struct AssetMetadata
	{
		AssetHandle Handle = k_InvalidAsset;
		AssetType Type = AssetType::None;
		// Relative to the AssetManager's root directory (kept relative so registrations
		// stay valid if the root moves between runs/machines).
		std::filesystem::path FilePath;
		AssetState State = AssetState::Unloaded;
		// Source-file timestamp at load; the hot-reload poll compares against it.
		std::filesystem::file_time_type LastWriteTime{};
	};

}
