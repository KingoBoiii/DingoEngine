#pragma once
#include "DingoEngine/Asset/AssetTypes.h"
#include "DingoEngine/Asset/AssetMetadata.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace Dingo
{

	class Texture;
	class Shader;
	class Model;
	class Font;
	class AudioClip;
	class AudioEngine;

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
	// file and re-creating GPU resources like the raw Create factories do.
	//
	// The manager owns every object it loads and frees them all at Shutdown() — game
	// code holds handles (or borrowed pointers) and never deletes managed assets.
	// The raw factories (Texture::Create, Model::LoadFromFile, ...) remain available
	// for unmanaged resources such as render targets or generated data.
	//
	// Load failures keep the asset registered with State == Failed (Get returns
	// nullptr) so a later Reload — or hot-reload, once the file is fixed — can recover
	// without re-registering.
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

		// Import + synchronous load. Returns the handle even if loading fails (the
		// registration stays, State == Failed). Does not block on an asset already
		// loading in the background - poll IsReady() for those.
		AssetHandle Load(const std::filesystem::path& path);

		// Import + background load: returns immediately, Get returns nullptr until the
		// asset is Ready. Textures and audio clips decode on the loader thread and are
		// finalized (GPU upload / publish) in Update(); other types fall back to a
		// main-thread load inside Update(), one asset per frame, so a loading screen
		// keeps rendering between them. Poll IsReady() / GetPendingCount().
		AssetHandle LoadAsync(const std::filesystem::path& path);

		// Synchronously (re)loads an already-registered asset. Returns true when the
		// asset is Ready afterwards.
		bool Reload(AssetHandle handle);

		// Frees the loaded object but keeps the registration (State -> Unloaded).
		// Any borrowed pointers to the object go stale — callers re-Get after a reload.
		void Unload(AssetHandle handle);

		// Unload + forget the registration entirely.
		void Remove(AssetHandle handle);

		// --- Access ---------------------------------------------------------

		// nullptr unless the asset is Ready and of the requested type.
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

		bool IsReady(AssetHandle handle) const { return GetState(handle) == AssetState::Ready; }
		// AssetState::Unloaded for unknown handles.
		AssetState GetState(AssetHandle handle) const;
		// nullptr for unknown handles.
		const AssetMetadata* GetMetadata(AssetHandle handle) const;
		// k_InvalidAsset if the path was never imported.
		AssetHandle FindByPath(const std::filesystem::path& path) const;

		const std::filesystem::path& GetRootDirectory() const { return m_RootDirectory; }
		std::filesystem::path ResolvePath(const std::filesystem::path& path) const;

		uint32_t GetRegisteredCount() const { return static_cast<uint32_t>(m_Registry.size()); }
		uint32_t GetLoadedCount() const;
		// Async loads still in flight (queued, decoding, or awaiting finalize).
		uint32_t GetPendingCount() const { return m_PendingCount.load(); }

		// Per-frame pump, driven by Application::Run — finalizes background loads and
		// polls hot-reload watches.
		void Update(float deltaTime);

	private:
		template<typename>
		static inline constexpr bool k_AlwaysFalseAsset = false;

		// The worker thread only ever sees these two structs - it never touches the
		// registry or the asset maps, so all registry state stays main-thread-owned.
		struct AsyncJob
		{
			AssetHandle Handle = k_InvalidAsset;
			AssetType Type = AssetType::None;
			std::filesystem::path AbsolutePath;
			std::string DebugName;
		};

		struct AsyncResult
		{
			AssetHandle Handle = k_InvalidAsset;
			AssetType Type = AssetType::None;
			bool Success = false;
			const uint8_t* Pixels = nullptr; // decoded texture payload (stbi buffer)
			uint32_t Width = 0;
			uint32_t Height = 0;
			std::shared_ptr<AudioClip> Clip; // decoded audio payload
			std::string DebugName;
		};

		bool LoadInternal(AssetMetadata& metadata);
		void UnloadInternal(const AssetMetadata& metadata);
		void WorkerLoop();
		void FinalizeResult(AsyncResult& result);
		void QueueDecodeJob(const AssetMetadata& metadata);
		void PollHotReload();
		// The canonical registry/lookup key for a path.
		static std::string NormalizePathKey(const std::filesystem::path& path);

	private:
		AssetManagerParams m_Params;
		std::filesystem::path m_RootDirectory;
		AudioEngine* m_AudioEngine = nullptr;

		std::unordered_map<AssetHandle, AssetMetadata> m_Registry;
		std::unordered_map<std::string, AssetHandle> m_PathLookup;

		std::unordered_map<AssetHandle, Texture*> m_Textures;
		std::unordered_map<AssetHandle, Shader*> m_Shaders;
		std::unordered_map<AssetHandle, Model*> m_Models;
		std::unordered_map<AssetHandle, Font*> m_Fonts;
		std::unordered_map<AssetHandle, std::shared_ptr<AudioClip>> m_AudioClips;

		std::thread m_Worker;
		std::mutex m_JobMutex;
		std::condition_variable m_JobCV;
		std::deque<AsyncJob> m_Jobs;     // guarded by m_JobMutex
		bool m_StopWorker = false;       // guarded by m_JobMutex
		std::mutex m_ResultMutex;
		std::vector<AsyncResult> m_Results; // guarded by m_ResultMutex
		// Async requests for types whose loaders create GPU resources internally
		// (Shader/Model/Font) - drained one per Update() on the main thread.
		std::deque<AssetHandle> m_MainThreadQueue;
		// LoadClip is never issued from two threads at once (worker vs. a sync Load).
		std::mutex m_AudioLoadMutex;
		std::atomic<uint32_t> m_PendingCount = 0;
		float m_HotReloadTimer = 0.0f;
	};

}
