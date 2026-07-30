#pragma once
#include "DingoEngine/Asset/AssetTypes.h"
#include "DingoEngine/Asset/AssetMetadata.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <type_traits>
#include <unordered_map>

namespace Dingo
{

	class Texture;
	class Shader;
	class Model;
	class Font;
	class AudioClip;
	class AudioEngine;

	namespace Internal { struct AssetManagerData; }

	struct AssetManagerParams
	{
		// Every asset path resolves against this root. A relative root is resolved
		// against the working directory once at Initialize(), so a game can anchor
		// assets to e.g. the executable's directory instead of depending on where it
		// was launched from (the classic cwd-relative asset trap).
		std::filesystem::path RootDirectory = "assets";

		// Watches loaded textures and shaders for on-disk changes and reloads them in
		// place (existing Texture*/Shader* pointers stay valid; pipelines rebuild
		// lazily). A shader compile error keeps the previous program running. Intended
		// for development - leave off in shipping builds.
		bool EnableHotReload = false;
		// Seconds between file-timestamp polls when hot-reload is enabled.
		float HotReloadInterval = 0.5f;

		AssetManagerParams& SetRootDirectory(const std::filesystem::path& root)
		{
			RootDirectory = root;
			return *this;
		}

		AssetManagerParams& SetEnableHotReload(bool enable)
		{
			EnableHotReload = enable;
			return *this;
		}

		AssetManagerParams& SetHotReloadInterval(float seconds)
		{
			HotReloadInterval = seconds;
			return *this;
		}
	};

	// Central registry and owner of file-backed assets. Assets are identified by a
	// stable AssetHandle (UUID) and deduplicated by path: loading the same file twice
	// returns the same handle and the same loaded object, instead of re-reading the
	// file and re-creating GPU resources like the raw Create factories do. Paths are
	// deduplicated by their normalized spelling, so an absolute path under the asset
	// root, its relative form and (on Windows) any capitalization of either all name
	// the same registration.
	//
	// The manager owns every object it loads and frees them all at Shutdown() — game
	// code holds handles (or borrowed pointers) and never deletes managed assets.
	// The raw factories (Texture::Create, Model::LoadFromFile, ...) remain available
	// for unmanaged resources such as render targets or generated data.
	//
	// Load failures keep the asset registered with State == Failed (Get returns
	// nullptr) so a later Reload — or hot-reload, once the file is fixed — can recover
	// without re-registering.
	//
	// Threading: every member is main-thread-only (the thread driving Application::Run,
	// where Update() finalizes background loads), with GetPendingCount() and
	// GetReloadingCount() the only exceptions — both read an atomic. Loads decode on an
	// internal worker thread, but that thread never touches the registry or a managed
	// object, so calling e.g. GetTexture() from a job thread races the finalize pump's
	// map insertions.
	class AssetManager
	{
	public:
		// The audio engine is borrowed for AudioClip decoding; clips are released in
		// Shutdown(), which must run before the audio engine itself shuts down.
		AssetManager(const AssetManagerParams& params, AudioEngine* audioEngine);
		~AssetManager();

		void Initialize();
		void Shutdown();

		// --- Registration & loading ----------------------------------------

		// Registers `path` (relative to the asset root; absolute paths are used as-is)
		// without loading it, inferring the asset type from the extension. Importing an
		// already-registered path returns its existing handle. Returns k_InvalidAsset
		// for unrecognized extensions.
		AssetHandle Import(const std::filesystem::path& path);

		// Import + synchronous load: the asset is Ready when the call returns, unless
		// loading fails (the registration stays, State == Failed) or a background load of
		// the same path is already in flight — this does not block on those, so poll
		// IsReady() when mixing Load and LoadAsync for one asset.
		AssetHandle Load(const std::filesystem::path& path);

		// Import + background load: returns immediately, Get returns nullptr until the
		// asset is Ready. Textures and audio clips decode on the loader thread and are
		// finalized (GPU upload / publish) in Update(); other types fall back to a
		// main-thread load inside Update(), one asset per frame, so a loading screen
		// keeps rendering between them. Poll IsReady() / GetPendingCount().
		AssetHandle LoadAsync(const std::filesystem::path& path);

		// Synchronously (re)loads an already-registered asset. Returns true when the
		// asset is Ready afterwards.
		//
		// Loaded textures and shaders are refreshed IN PLACE (as hot-reload does), so
		// pointers already handed out stay valid and a failed reload keeps serving the
		// previously loaded version. Every other type is destroyed and recreated, which
		// invalidates borrowed pointers - re-Get after reloading those. Check
		// SupportsInPlaceReload(type) first if you need to know which applies without
		// calling this.
		//
		// A no-op while a load or reload of the same asset is already in flight: the
		// pending pass publishes the file as it stands on disk anyway, and cancelling it
		// would throw away a finished decode for nothing. It returns IsReady(handle) in that
		// case, so false there means "not loaded yet", not "the reload failed" — and with
		// hot-reload off nothing re-polls afterwards, so an edit made *during* that first load
		// is not picked up. Re-Reload once the asset is Ready if that matters.
		bool Reload(AssetHandle handle);

		// Frees the loaded object but keeps the registration (State -> Unloaded).
		// Any borrowed pointers to the object go stale — callers re-Get after a reload.
		void Unload(AssetHandle handle);

		// Unload + forget the registration entirely.
		void Remove(AssetHandle handle);

		// --- Access ---------------------------------------------------------

		// The loaded object of the requested type, or nullptr while the asset has none:
		// never loaded, still loading, failed, or unloaded. An in-place reload
		// (State == Reloading) keeps returning the live pointer — the object is refreshed,
		// never replaced, so a hot-reload never takes an asset away mid-frame.
		Texture* GetTexture(AssetHandle handle) const;
		Shader* GetShader(AssetHandle handle) const;
		Model* GetModel(AssetHandle handle) const;
		Font* GetFont(AssetHandle handle) const;
		std::shared_ptr<AudioClip> GetAudioClip(AssetHandle handle) const;

		template<typename T>
		T* Get(AssetHandle handle) const
		{
			if constexpr (std::is_same_v<T, Texture>)
				return GetTexture(handle);
			else if constexpr (std::is_same_v<T, Shader>)
				return GetShader(handle);
			else if constexpr (std::is_same_v<T, Model>)
				return GetModel(handle);
			else if constexpr (std::is_same_v<T, Font>)
				return GetFont(handle);
			else
				static_assert(k_AlwaysFalseAsset<T>, "AssetManager::Get<T> supports Texture, Shader, Model and Font; use GetAudioClip for audio (shared ownership)");
		}

		// --- Queries ----------------------------------------------------------

		// True once the asset is usable — including while an in-place reload is in flight,
		// where the previously loaded object keeps serving until the new one is published.
		bool IsReady(AssetHandle handle) const
		{
			const AssetState state = GetState(handle);
			return state == AssetState::Ready || state == AssetState::Reloading;
		}
		// AssetState::Unloaded for unknown handles.
		AssetState GetState(AssetHandle handle) const;
		// nullptr for unknown handles.
		const AssetMetadata* GetMetadata(AssetHandle handle) const;
		// k_InvalidAsset if the path was never imported.
		AssetHandle FindByPath(const std::filesystem::path& path) const;

		// True for exactly the types ReloadInPlace can refresh without destroying the
		// loaded object (Texture2D and Shader today). Tooling that offers a Reload
		// action - such as the debug panel - should check this first: Reload() on any
		// other type destroys and recreates the object, invalidating borrowed pointers.
		static bool SupportsInPlaceReload(AssetType type);

		const std::filesystem::path& GetRootDirectory() const;
		std::filesystem::path ResolvePath(const std::filesystem::path& path) const;

		// Every registration, for tooling (the built-in Assets debug panel walks this).
		// Handles stay valid across loads, so entries can be acted on while iterating a
		// copy of the keys.
		const std::unordered_map<AssetHandle, AssetMetadata>& GetRegistry() const;

		bool IsHotReloadEnabled() const;
		// Toggleable at runtime so a debug panel can turn watching on for a session
		// without a rebuild. Enabling re-arms the poll timer.
		void SetHotReloadEnabled(bool enable);

		uint32_t GetRegisteredCount() const;
		uint32_t GetLoadedCount() const;
		// LoadAsync requests still in flight (queued, decoding, or awaiting finalize).
		// Hot-reloads are deliberately excluded so a mid-game reload cannot stall a
		// loading screen gating on this. The one hole that leaves: a LoadAsync for a Failed
		// asset whose hot-reload retry is already in flight early-returns, so that asset is
		// counted by GetReloadingCount() and this can read 0 while it still has no object.
		// Pair the gate with IsReady() if failed assets can be retried mid-load.
		uint32_t GetPendingCount() const;
		// Hot-reload requests still in flight, including retries of a Failed asset. One that
		// still has a loaded object (State == Reloading) keeps serving it throughout.
		uint32_t GetReloadingCount() const;

		// Per-frame pump, driven by Application::Run — finalizes background loads and
		// polls hot-reload watches.
		void Update(float deltaTime);

	private:
		template<typename>
		static inline constexpr bool k_AlwaysFalseAsset = false;

		// Threading, containers and the per-type load policy live in
		// src/DingoEngine/Asset/AssetManagerData.h, so client code neither compiles them
		// nor rebuilds when they change.
		std::unique_ptr<Internal::AssetManagerData> m_Data;
	};

}
