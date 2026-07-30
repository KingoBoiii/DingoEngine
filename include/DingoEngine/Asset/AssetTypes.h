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
		AudioClip,

		// Not a type: the enumerator count, so a per-type table can static_assert that it
		// has a row for every type rather than silently serving the None row for a new one.
		Count
	};

	enum class AssetState : uint8_t
	{
		Unloaded = 0,	// registered, no loaded object
		Queued,			// waiting for a background loader
		Loading,		// being decoded on a worker thread
		Ready,			// loaded and usable
		Failed,			// last load attempt failed (error is logged); a reload can recover

		// Loaded and usable, with a newer version decoding in the background. The object is
		// refreshed in place when that lands, so borrowed pointers stay valid and IsReady()
		// stays true - a hot-reload must never make a sprite blink out. Appended rather than
		// placed in lifecycle order so the states above keep the values tooling indexes
		// per-state tables with.
		Reloading,

		// Not a state: the enumerator count, so a per-state array cannot silently overflow
		// when a state is added.
		Count
	};

	const char* AssetTypeToString(AssetType type);
	const char* AssetStateToString(AssetState state);

	// Infers the asset type from a file extension (".png" -> Texture2D, ".glsl" -> Shader,
	// ".obj"/".gltf" -> Model, ".ttf" -> Font, ".wav"/".ogg" -> AudioClip). Case-insensitive.
	// Returns AssetType::None for unrecognized extensions.
	AssetType AssetTypeFromExtension(const std::filesystem::path& extension);

}
