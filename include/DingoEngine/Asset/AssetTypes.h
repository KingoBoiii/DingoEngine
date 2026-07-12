#pragma once
#include "DingoEngine/Core/UUID.h"

#include <filesystem>

namespace Dingo
{

	// A stable 64-bit identifier for a registered asset. Handles survive unload/reload
	// cycles (including hot-reload), so game code can hold them across an asset's whole
	// lifetime instead of tracking raw object pointers.
	using AssetHandle = UUID;

	// The null handle: returned when registration fails, never assigned to a real asset.
	inline const AssetHandle k_InvalidAsset = AssetHandle(0);

	inline bool IsValidAssetHandle(AssetHandle handle) { return static_cast<uint64_t>(handle) != 0; }

	enum class AssetType : uint8_t
	{
		None = 0,
		Texture2D,
		Shader,
		Model,
		Font,
		AudioClip
	};

	enum class AssetState : uint8_t
	{
		Unloaded = 0,	// registered, no loaded object
		Queued,			// waiting for a background loader
		Loading,		// being decoded on a worker thread
		Ready,			// loaded and usable
		Failed			// last load attempt failed (error is logged); a reload can recover
	};

	const char* AssetTypeToString(AssetType type);

	// Infers the asset type from a file extension (".png" -> Texture2D, ".glsl" -> Shader,
	// ".obj"/".gltf" -> Model, ".ttf" -> Font, ".wav"/".ogg" -> AudioClip). Case-insensitive.
	// Returns AssetType::None for unrecognized extensions.
	AssetType AssetTypeFromExtension(const std::filesystem::path& extension);

}
