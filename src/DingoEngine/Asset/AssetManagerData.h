#pragma once

// Engine-internal: this header lives under src/ and is NEVER shipped or included by
// client code. It keeps the manager's threading primitives and containers out of the
// public header, which Application.h pulls into every client translation unit.

#include "DingoEngine/Asset/AssetManager.h"
#include "DingoEngine/Asset/AssetMetadata.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
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

	namespace Internal
	{

		// The loader thread's input and output: a deep copy of everything a decode needs, and
		// the payload the main-thread pump publishes from. What a decode is allowed to touch
		// is stated on AssetTypePolicy::Decode.
		struct AsyncJob
		{
			AssetHandle Handle = k_InvalidAsset;
			AssetType Type = AssetType::None;
			std::filesystem::path AbsolutePath;
			std::string DebugName;
			// Hot-reload traffic is counted apart from LoadAsync requests, so it travels with
			// the job: only the thread that finalizes it knows which counter to release.
			bool IsReload = false;
		};

		struct AsyncResult
		{
			AssetHandle Handle = k_InvalidAsset;
			AssetType Type = AssetType::None;
			bool Success = false;
			const uint8_t* Pixels = nullptr; // decoded texture payload (stbi buffer)
			uint32_t Width = 0;
			uint32_t Height = 0;
			uint32_t Channels = 0;
			std::shared_ptr<AudioClip> Clip; // decoded audio payload
			std::string DebugName;
			bool IsReload = false;
		};

		struct AssetManagerData
		{
			AssetManagerParams Params;
			std::filesystem::path RootDirectory;
			// No mutex around LoadClip: ma_engine owns a resource manager whose job queue is
			// multi-producer/multi-consumer, and node attachment is spinlock-guarded, so
			// miniaudio supports initializing sounds from several threads at once. Serializing
			// it here only made a sync Load block the main thread on the worker's decode.
			AudioEngine* Audio = nullptr;

			std::unordered_map<AssetHandle, AssetMetadata> Registry;
			// Keyed by the folded path key, not by AssetMetadata::FilePath, which keeps its
			// original casing - see NormalizePathKey.
			std::unordered_map<std::string, AssetHandle> PathLookup;

			std::unordered_map<AssetHandle, Texture*> Textures;
			std::unordered_map<AssetHandle, Shader*> Shaders;
			std::unordered_map<AssetHandle, Model*> Models;
			std::unordered_map<AssetHandle, Font*> Fonts;
			std::unordered_map<AssetHandle, std::shared_ptr<AudioClip>> AudioClips;

			std::thread Worker;
			std::mutex JobMutex;
			std::condition_variable JobCV;
			std::deque<AsyncJob> Jobs;			// guarded by JobMutex
			bool StopWorker = false;			// guarded by JobMutex
			std::mutex ResultMutex;
			std::vector<AsyncResult> Results;	// guarded by ResultMutex
			// Async requests for types whose loaders create GPU resources internally
			// (Shader/Model/Font) - drained one per Update() on the main thread.
			std::deque<AssetHandle> MainThreadQueue;

			std::atomic<uint32_t> PendingCount = 0;
			// Reloads are tracked apart from PendingCount: games gate their loading screen on
			// GetPendingCount(), and a mid-game hot-reload must not stall that bar.
			std::atomic<uint32_t> ReloadCount = 0;
			float HotReloadTimer = 0.0f;

			// Round-robin state for PollHotReload: the watchable handles for the pass in
			// progress, and how far through them the last poll got.
			std::vector<AssetHandle> WatchList;
			std::size_t WatchCursor = 0;
		};

	}

}
