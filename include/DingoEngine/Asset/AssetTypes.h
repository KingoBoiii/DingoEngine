#pragma once
#include "DingoEngine/Core/UUID.h"

#include <filesystem>

namespace Dingo
{

	/// A stable 64-bit identifier for a registered asset. Handles survive unload/reload
	/// cycles (including hot-reload), so game code can hold them across an asset's whole
	/// lifetime instead of tracking raw object pointers.
	using AssetHandle = UUID;

	/// The null handle: returned when registration fails, never assigned to a real asset.
	inline constexpr AssetHandle k_InvalidAsset{};

	/// True for any handle but k_InvalidAsset; says nothing about whether the asset is still
	/// registered.
	inline bool IsValidAssetHandle(AssetHandle handle) { return static_cast<uint64_t>(handle) != 0; }

	/// The kind of file an asset is, inferred from its extension (AssetTypeFromExtension).
	enum class AssetType : uint8_t
	{
		None = 0,	///< Unrecognized extension; such a file can't be imported.
		Texture2D,
		Shader,
		Model,
		Font,
		AudioClip,

		/// Not a type: the enumerator count, so a per-type table can static_assert that it
		/// has a row for every type rather than silently serving the None row for a new one.
		Count
	};

	/// Where an asset is in its load lifecycle (AssetManager::GetState).
	enum class AssetState : uint8_t
	{
		Unloaded = 0,	///< Registered, no loaded object.
		Queued,			///< Waiting for a background loader.
		Loading,		///< Being decoded on a worker thread.
		Ready,			///< Loaded and usable.
		Failed,			///< Last load attempt failed (error is logged); a reload can recover.

		/// Loaded and usable, with a newer version decoding in the background. The object is
		/// refreshed in place when that lands, so borrowed pointers stay valid and IsReady()
		/// stays true - a hot-reload must never make a sprite blink out. Appended rather than
		/// placed in lifecycle order so the states above keep the values tooling indexes
		/// per-state tables with.
		Reloading,

		/// Not a state: the enumerator count, so a per-state array cannot silently overflow
		/// when a state is added.
		Count
	};

	/// The type's name, such as "Texture2D"; "Unknown" for Count or an invalid value. Static
	/// storage.
	const char* ToString(AssetType type);
	/// The state's name, such as "Ready"; "Unknown" for Count or an invalid value. Static storage.
	const char* ToString(AssetState state);

	/// Deprecated alias of ToString(AssetType); removed in 2.0.
	[[deprecated("use ToString(AssetType); removed in 2.0")]] inline const char* AssetTypeToString(AssetType type) { return ToString(type); }
	/// Deprecated alias of ToString(AssetState); removed in 2.0.
	[[deprecated("use ToString(AssetState); removed in 2.0")]] inline const char* AssetStateToString(AssetState state) { return ToString(state); }

	/// Infers the asset type from a file extension (".png" -> Texture2D, ".glsl" -> Shader,
	/// ".obj"/".gltf" -> Model, ".ttf" -> Font, ".wav"/".ogg" -> AudioClip). Case-insensitive.
	/// Returns AssetType::None for unrecognized extensions.
	/// All of them: .png .jpg .jpeg .tga .bmp (Texture2D); .glsl .shader (Shader); .obj .gltf .glb
	/// .fbx (Model); .ttf .otf (Font); .wav .ogg .mp3 (AudioClip).
	AssetType AssetTypeFromExtension(const std::filesystem::path& extension);

}
