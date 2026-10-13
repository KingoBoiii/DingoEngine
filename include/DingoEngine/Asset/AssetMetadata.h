#pragma once
#include "DingoEngine/Asset/AssetTypes.h"

#include <filesystem>

namespace Dingo
{

	/// What the AssetManager knows about one registered asset (AssetManager::GetMetadata).
	struct AssetMetadata
	{
		AssetHandle Handle = k_InvalidAsset;	///< The asset's stable handle.
		AssetType Type = AssetType::None;	///< Inferred from the file extension at Import.
		/// Relative to the AssetManager's root directory (kept relative so registrations
		/// stay valid if the root moves between runs/machines).
		std::filesystem::path FilePath;
		AssetState State = AssetState::Unloaded;	///< Where the asset is in its load lifecycle.
	};
}
