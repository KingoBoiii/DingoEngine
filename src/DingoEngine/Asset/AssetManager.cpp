#include "depch.h"
#include "DingoEngine/Asset/AssetManager.h"

#include "DingoEngine/Asset/AssetManagerData.h"
#include "DingoEngine/Core/FileSystem.h"
#include "DingoEngine/Graphics/Texture.h"
#include "DingoEngine/Graphics/Shader.h"
#include "DingoEngine/Graphics/Model.h"
#include "DingoEngine/Graphics/Font.h"
#include "DingoEngine/Audio/AudioEngine.h"

#include <algorithm>
#include <cctype>

namespace Dingo
{

	using Internal::AssetManagerData;
	using Internal::AsyncJob;
	using Internal::AsyncResult;

	namespace Utils
	{

		// Shader and font names end up in cache-file names (.cache/shaders/<name>_*.spv,
		// .cache/fonts/...), so path separators must not survive into them.
		static std::string SanitizeAssetName(const std::filesystem::path& relativePath)
		{
			std::string name = relativePath.generic_string();
			for (char& c : name)
			{
				if (c == '/' || c == '\\' || c == ':')
					c = '_';
			}
			return name;
		}

		// NTFS is case-insensitive: without folding, "Textures/Player.png" and
		// "textures/player.png" would each get their own handle, GPU texture and hot-reload
		// watch for one file, and FindByPath would miss. Elsewhere the two are genuinely
		// different files.
		static std::string FoldPathCase(std::string key)
		{
#ifdef DE_PLATFORM_WINDOWS
			std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
#endif
			return key;
		}

		static bool PathPrefixMatches(std::string_view path, std::string_view prefix)
		{
			if (path.size() <= prefix.size() || prefix.empty())
				return false;
			if (path[prefix.size()] != '/')
				return false;

			const std::string_view head = path.substr(0, prefix.size());
#ifdef DE_PLATFORM_WINDOWS
			return std::equal(head.begin(), head.end(), prefix.begin(), [](unsigned char l, unsigned char r) { return std::tolower(l) == std::tolower(r); });
#else
			return head == prefix;
#endif
		}

		static void StampWriteTime(AssetMetadata& metadata, const std::filesystem::path& absolutePath)
		{
			std::error_code ec;
			metadata.LastWriteTime = std::filesystem::last_write_time(absolutePath, ec);
			if (ec)
				metadata.LastWriteTime = {};
		}

		static TextureParams MakeTextureParams(const std::string& debugName, uint32_t width, uint32_t height, uint32_t channels, const uint8_t* pixels)
		{
			return TextureParams()
				.SetDebugName(debugName)
				.SetWidth(width)
				.SetHeight(height)
				.SetDimension(TextureDimension::Texture2D)
				.SetFormat(channels == 4 ? TextureFormat::RGBA : TextureFormat::RGB)
				.SetIsRenderTarget(false)
				.SetInitialData(pixels);
		}

		template<typename T>
		static void DestroyFrom(std::unordered_map<AssetHandle, T*>& assets, AssetHandle handle)
		{
			auto it = assets.find(handle);
			if (it == assets.end())
				return;

			it->second->Destroy();
			delete it->second;
			assets.erase(it);
		}

		template<typename T>
		static void DestroyAll(std::unordered_map<AssetHandle, T*>& assets)
		{
			for (auto& [handle, asset] : assets)
			{
				asset->Destroy();
				delete asset;
			}
			assets.clear();
		}

	}

	// What an in-place refresh did, which decides whether the caller may commit a new
	// write-time stamp: a refresh that kept the previous contents must not, or the edit
	// that failed to load becomes invisible to every later poll.
	enum class RefreshResult
	{
		NotLoaded,		// nothing loaded to refresh - the caller has to load instead
		KeptPrevious,	// refresh failed; the previously loaded object keeps serving
		Refreshed
	};

	// Everything type-specific about an asset, in one row per AssetType. A null slot is
	// the answer "this type does not do that": no Load, no in-place refresh, no worker
	// decode. SupportsInPlaceReload is read straight off ReloadInPlace, so the public
	// promise and the implementation cannot drift apart.
	struct AssetTypePolicy
	{
		AssetType Type = AssetType::None;

		// Creates the object and stores it; the caller owns stamping and the state flip.
		bool (*Load)(AssetManagerData&, const AssetMetadata&) = nullptr;
		// Refreshes the loaded object's contents without replacing it, so pointers already
		// handed out stay valid. Null for types that can only be destroyed and recreated.
		RefreshResult (*ReloadInPlace)(AssetManagerData&, const AssetMetadata&) = nullptr;
		void (*Unload)(AssetManagerData&, AssetHandle) = nullptr;
		bool (*IsLoaded)(const AssetManagerData&, AssetHandle) = nullptr;
		// Decode runs on the loader thread and must touch nothing beyond the audio engine
		// pointer, which is fixed for the manager's lifetime - all registry and asset-map
		// state stays main-thread-owned. Publish applies the payload in the main-thread
		// pump. Both null for types that load synchronously.
		void (*Decode)(AssetManagerData&, const AsyncJob&, AsyncResult&) = nullptr;
		void (*Publish)(AssetManagerData&, AsyncResult&) = nullptr;

		bool HotReloadWatched = false;
	};

	static const AssetTypePolicy& PolicyFor(AssetType type)
	{
		// Indexed by AssetType, so row 0 is None: every slot null, which is the same
		// "nothing to do" path the per-type switches used to reach via `default`.
		static const AssetTypePolicy s_Policies[] =
		{
			{
				.Type = AssetType::None
			},
			{
				.Type = AssetType::Texture2D,
				.Load = [](AssetManagerData& data, const AssetMetadata& metadata) -> bool
				{
					Texture* texture = Texture::CreateFromFile(metadata.AbsolutePath, metadata.FilePath.generic_string());
					if (!texture)
						return false;

					data.Textures[metadata.Handle] = texture;
					return true;
				},
				.ReloadInPlace = [](AssetManagerData& data, const AssetMetadata& metadata) -> RefreshResult
				{
					auto it = data.Textures.find(metadata.Handle);
					if (it == data.Textures.end())
						return RefreshResult::NotLoaded;

					uint32_t width = 0, height = 0, channels = 0;
					const uint8_t* pixels = FileSystem::ReadImage(metadata.AbsolutePath, &width, &height, &channels, true, true);
					if (!pixels)
					{
						// Keep serving the currently loaded image, as a failed hot-reload does.
						DE_CORE_ERROR("AssetManager: reload failed for Texture2D '{}' - keeping the loaded version.", metadata.FilePath.generic_string());
						return RefreshResult::KeptPrevious;
					}

					it->second->Reinitialize(Utils::MakeTextureParams(metadata.FilePath.generic_string(), width, height, channels, pixels));
					FileSystem::FreeImage(pixels);
					return RefreshResult::Refreshed;
				},
				.Unload = [](AssetManagerData& data, AssetHandle handle) { Utils::DestroyFrom(data.Textures, handle); },
				.IsLoaded = [](const AssetManagerData& data, AssetHandle handle) -> bool { return data.Textures.contains(handle); },
				.Decode = [](AssetManagerData&, const AsyncJob& job, AsyncResult& result)
				{
					uint32_t width = 0, height = 0, channels = 0;
					result.Pixels = FileSystem::ReadImage(job.AbsolutePath, &width, &height, &channels, true, true);
					result.Width = width;
					result.Height = height;
					result.Channels = channels;
					result.Success = result.Pixels != nullptr;
				},
				.Publish = [](AssetManagerData& data, AsyncResult& result)
				{
					const TextureParams textureParams = Utils::MakeTextureParams(result.DebugName, result.Width, result.Height, result.Channels, result.Pixels);

					auto existing = data.Textures.find(result.Handle);
					if (existing != data.Textures.end())
					{
						// Hot-reload: swap the contents inside the same object so every
						// Texture* held by game code keeps working.
						existing->second->Reinitialize(textureParams);
					}
					else
					{
						data.Textures[result.Handle] = Texture::Create(textureParams);
					}
				},
				.HotReloadWatched = true
			},
			{
				.Type = AssetType::Shader,
				.Load = [](AssetManagerData& data, const AssetMetadata& metadata) -> bool
				{
					// Shader::Create never returns nullptr - a failed compile/read yields an
					// object with no program, detected via IsValid().
					Shader* shader = Shader::CreateFromFile(Utils::SanitizeAssetName(metadata.FilePath), metadata.AbsolutePath);
					if (shader && !shader->IsValid())
					{
						shader->Destroy();
						delete shader;
						shader = nullptr;
					}
					if (!shader)
						return false;

					data.Shaders[metadata.Handle] = shader;
					return true;
				},
				.ReloadInPlace = [](AssetManagerData& data, const AssetMetadata& metadata) -> RefreshResult
				{
					auto it = data.Shaders.find(metadata.Handle);
					if (it == data.Shaders.end())
						return RefreshResult::NotLoaded;

					it->second->Reload(); // keeps the previous program on a compile error
					return RefreshResult::Refreshed;
				},
				.Unload = [](AssetManagerData& data, AssetHandle handle) { Utils::DestroyFrom(data.Shaders, handle); },
				.IsLoaded = [](const AssetManagerData& data, AssetHandle handle) -> bool { return data.Shaders.contains(handle); },
				.HotReloadWatched = true
			},
			{
				.Type = AssetType::Model,
				.Load = [](AssetManagerData& data, const AssetMetadata& metadata) -> bool
				{
					Model* model = Model::LoadFromFile(metadata.AbsolutePath);
					if (!model)
						return false;

					data.Models[metadata.Handle] = model;
					return true;
				},
				.Unload = [](AssetManagerData& data, AssetHandle handle) { Utils::DestroyFrom(data.Models, handle); },
				.IsLoaded = [](const AssetManagerData& data, AssetHandle handle) -> bool { return data.Models.contains(handle); }
			},
			{
				.Type = AssetType::Font,
				.Load = [](AssetManagerData& data, const AssetMetadata& metadata) -> bool
				{
					FontParams fontParams;
					fontParams.Name = Utils::SanitizeAssetName(metadata.FilePath);

					Font* font = Font::Create(metadata.AbsolutePath, fontParams);
					if (!font)
						return false;

					data.Fonts[metadata.Handle] = font;
					return true;
				},
				.Unload = [](AssetManagerData& data, AssetHandle handle) { Utils::DestroyFrom(data.Fonts, handle); },
				.IsLoaded = [](const AssetManagerData& data, AssetHandle handle) -> bool { return data.Fonts.contains(handle); }
			},
			{
				.Type = AssetType::AudioClip,
				.Load = [](AssetManagerData& data, const AssetMetadata& metadata) -> bool
				{
					DE_CORE_ASSERT(data.Audio, "AssetManager has no audio engine - cannot load audio clips");
					std::shared_ptr<AudioClip> clip = data.Audio->LoadClip(metadata.AbsolutePath);
					if (!clip)
						return false;

					data.AudioClips[metadata.Handle] = std::move(clip);
					return true;
				},
				.Unload = [](AssetManagerData& data, AssetHandle handle) { data.AudioClips.erase(handle); },
				.IsLoaded = [](const AssetManagerData& data, AssetHandle handle) -> bool { return data.AudioClips.contains(handle); },
				.Decode = [](AssetManagerData& data, const AsyncJob& job, AsyncResult& result)
				{
					result.Clip = data.Audio->LoadClip(job.AbsolutePath);
					result.Success = result.Clip != nullptr;
				},
				.Publish = [](AssetManagerData& data, AsyncResult& result)
				{
					data.AudioClips[result.Handle] = std::move(result.Clip);
				}
			}
		};

		// Indexed by AssetType, so a missing row would silently serve the None row (no load,
		// no unload, no in-place reload) in every build. Caught here at compile time instead.
		static_assert(std::size(s_Policies) == static_cast<std::size_t>(AssetType::Count),
			"AssetManager: s_Policies needs exactly one row per AssetType, in AssetType order");

		const std::size_t index = static_cast<std::size_t>(type);
		if (index >= std::size(s_Policies))
			return s_Policies[0];

		DE_CORE_ASSERT(s_Policies[index].Type == type, "AssetManager: policy rows must be listed in AssetType order");
		return s_Policies[index];
	}

	// The relative path a registration is stored under, with its original casing kept for
	// display and for the shader/font cache names. An absolute path under the asset root is
	// folded back to its relative spelling so both spellings share one registration.
	static std::filesystem::path NormalizeRelativePath(const AssetManagerData& data, const std::filesystem::path& path)
	{
		std::string generic = path.lexically_normal().generic_string();
		const std::string root = data.RootDirectory.generic_string();

		if (Utils::PathPrefixMatches(generic, root))
			generic.erase(0, root.size() + 1);

		return std::filesystem::path(generic);
	}

	// The canonical registry/lookup key for a path: the relative form above, case-folded on
	// filesystems that ignore case.
	static std::string NormalizePathKey(const AssetManagerData& data, const std::filesystem::path& path)
	{
		return Utils::FoldPathCase(NormalizeRelativePath(data, path).generic_string());
	}

	static bool LoadInternal(AssetManagerData& data, AssetMetadata& metadata)
	{
		const AssetTypePolicy& policy = PolicyFor(metadata.Type);
		if (policy.Load && (*policy.Load)(data, metadata))
		{
			Utils::StampWriteTime(metadata, metadata.AbsolutePath);
			metadata.State = AssetState::Ready;
			return true;
		}

		DE_CORE_ERROR("AssetManager: failed to load {} '{}'.", AssetTypeToString(metadata.Type), metadata.AbsolutePath.string());
		metadata.State = AssetState::Failed;
		return false;
	}

	// Refreshes a loaded asset's contents without replacing the object. False when the type
	// has no in-place path (the caller then destroys and recreates), which is the same policy
	// slot SupportsInPlaceReload reports.
	static bool ReloadInPlace(AssetManagerData& data, AssetMetadata& metadata)
	{
		const AssetTypePolicy& policy = PolicyFor(metadata.Type);
		if (!policy.ReloadInPlace)
			return false;

		const RefreshResult result = (*policy.ReloadInPlace)(data, metadata);
		if (result == RefreshResult::NotLoaded)
			return false;

		if (result == RefreshResult::Refreshed)
			Utils::StampWriteTime(metadata, metadata.AbsolutePath);

		return true;
	}

	static void UnloadInternal(AssetManagerData& data, const AssetMetadata& metadata)
	{
		const AssetTypePolicy& policy = PolicyFor(metadata.Type);
		if (policy.Unload)
			(*policy.Unload)(data, metadata.Handle);

		data.Registry.at(metadata.Handle).State = AssetState::Unloaded;
	}

	static void QueueDecodeJob(AssetManagerData& data, const AssetMetadata& metadata, bool isReload)
	{
		AsyncJob job;
		job.Handle = metadata.Handle;
		job.Type = metadata.Type;
		job.AbsolutePath = metadata.AbsolutePath;
		job.DebugName = metadata.FilePath.generic_string();
		job.IsReload = isReload;
		{
			std::scoped_lock lock(data.JobMutex);
			data.Jobs.push_back(std::move(job));
		}
		data.JobCV.notify_one();
	}

	static void WorkerLoop(AssetManagerData& data)
	{
		for (;;)
		{
			AsyncJob job;
			{
				std::unique_lock lock(data.JobMutex);
				data.JobCV.wait(lock, [&data] { return data.StopWorker || !data.Jobs.empty(); });
				if (data.StopWorker)
					return;

				job = std::move(data.Jobs.front());
				data.Jobs.pop_front();
			}

			AsyncResult result;
			result.Handle = job.Handle;
			result.Type = job.Type;
			result.DebugName = std::move(job.DebugName);
			result.IsReload = job.IsReload;

			// An exception escaping a thread function is a hard process kill, with no log line
			// and no chance to mark the asset Failed. The decoders reach std::filesystem
			// (miniaudio uses the throwing `exists` overload), which throws for a path the OS
			// rejects — a bad character or one over MAX_PATH. Route it back as a normal failure.
			try
			{
				if (const AssetTypePolicy& policy = PolicyFor(job.Type); policy.Decode)
					(*policy.Decode)(data, job, result);
			}
			catch (const std::exception& e)
			{
				result.Success = false;
				DE_CORE_ERROR("AssetManager: worker threw while decoding '{}': {}", result.DebugName, e.what());
			}
			catch (...)
			{
				result.Success = false;
				DE_CORE_ERROR("AssetManager: worker threw a non-std exception while decoding '{}'.", result.DebugName);
			}

			{
				std::scoped_lock lock(data.ResultMutex);
				data.Results.push_back(std::move(result));
			}
		}
	}

	static void FinalizeResult(AssetManagerData& data, AsyncResult& result)
	{
		auto it = data.Registry.find(result.Handle);
		// Only publish if the request is still wanted - Unload/Remove during the background
		// decode leaves a state that is neither, and the payload is discarded.
		const bool stillWanted = it != data.Registry.end()
			&& (it->second.State == AssetState::Loading || it->second.State == AssetState::Reloading);
		const AssetTypePolicy& policy = PolicyFor(result.Type);

		// Publish is what turns a payload into a loaded object, so a policy row carrying a
		// Decode without one has nowhere to put the result: count that as a failed load rather
		// than leaving the asset in flight forever with its counter already released.
		const bool published = stillWanted && result.Success && policy.Publish;
		if (published)
		{
			(*policy.Publish)(data, result);
			it->second.State = AssetState::Ready;
		}
		else if (stillWanted)
		{
			DE_CORE_ERROR("AssetManager: async load failed for {} '{}'.", AssetTypeToString(result.Type), result.DebugName);
			// A failed hot-reload keeps serving the still-loaded previous version.
			const bool hasLoadedObject = policy.IsLoaded && (*policy.IsLoaded)(data, result.Handle);
			it->second.State = hasLoadedObject ? AssetState::Ready : AssetState::Failed;
		}

		if (result.Pixels)
			FileSystem::FreeImage(result.Pixels);

		if (result.IsReload)
			--data.ReloadCount;
		else
			--data.PendingCount;
	}

	static void PollHotReload(AssetManagerData& data)
	{
		// Collect the watchable handles once per pass instead of walking the whole registry
		// every tick. Handles are stable, so an entry removed mid-pass just misses its turn.
		if (data.WatchCursor >= data.WatchList.size())
		{
			data.WatchList.clear();
			for (const auto& [handle, metadata] : data.Registry)
			{
				if (PolicyFor(metadata.Type).HotReloadWatched)
					data.WatchList.push_back(handle);
			}
			data.WatchCursor = 0;
		}

		// Cap the stat() calls a single poll can make. A project with fewer watched assets
		// than the cap still gets its whole list checked every tick, so nothing about
		// detection latency changes there; only projects big enough for the syscalls to cost
		// real frame time spread the work out, and they spread it in proportion. This also
		// bounds the cost when HotReloadInterval is 0, which polls every frame.
		static constexpr std::size_t k_MaxWatchChecksPerPoll = 64;
		const std::size_t passEnd = std::min(data.WatchList.size(), data.WatchCursor + k_MaxWatchChecksPerPoll);

		for (; data.WatchCursor < passEnd; ++data.WatchCursor)
		{
			auto entry = data.Registry.find(data.WatchList[data.WatchCursor]);
			if (entry == data.Registry.end())
				continue;

			AssetMetadata& metadata = entry->second;

			// Failed is watched as well as Ready: a first-load failure keeps its
			// registration precisely so fixing the file recovers it. Its write time was
			// never stamped, so the first poll fires immediately - and a file that is still
			// broken gets one attempt per edit, not one per poll, because the stamp below
			// lands whether or not the reload succeeds. Anything else (including an asset
			// with a reload already in flight) is skipped.
			const bool recovering = metadata.State == AssetState::Failed;
			if (metadata.State != AssetState::Ready && !recovering)
				continue;

			std::error_code ec;
			const auto writeTime = std::filesystem::last_write_time(metadata.AbsolutePath, ec);
			if (ec || writeTime == metadata.LastWriteTime)
			{
				metadata.PendingWriteTime = {};
				continue;
			}

			// Wait for the timestamp to repeat before believing the write finished. An editor
			// that truncates and then writes can be caught mid-save, and Windows resolves
			// mtime to one ~15.6 ms clock tick, so the partial file and the final file can
			// carry the SAME timestamp - reload the partial one and no later poll ever sees a
			// difference again, losing the edit until the next save. A file still being
			// written keeps moving its timestamp and simply waits here.
			if (metadata.PendingWriteTime != writeTime)
			{
				metadata.PendingWriteTime = writeTime;
				continue;
			}

			metadata.PendingWriteTime = {};
			metadata.LastWriteTime = writeTime;

			if (recovering)
				DE_CORE_INFO("AssetManager: retrying failed {} '{}' - the file changed on disk.", AssetTypeToString(metadata.Type), metadata.FilePath.generic_string());
			else
				DE_CORE_INFO("AssetManager: '{}' changed on disk - hot-reloading.", metadata.FilePath.generic_string());

			const AssetTypePolicy& policy = PolicyFor(metadata.Type);
			if (policy.Decode)
			{
				// Reloading, not Loading, whenever there is still an object to serve: IsReady()
				// stays true and Get* keeps handing out the same pointer while the new version
				// decodes, which is the whole promise of an in-place reload. A recovering Failed
				// asset has nothing to serve, so it is a plain Loading.
				const bool hasLoadedObject = policy.IsLoaded && (*policy.IsLoaded)(data, metadata.Handle);
				metadata.State = hasLoadedObject ? AssetState::Reloading : AssetState::Loading;
				++data.ReloadCount;
				QueueDecodeJob(data, metadata, true);
				continue;
			}

			// The stamp above is already committed, so the refresh must not stamp again.
			const RefreshResult refreshed = policy.ReloadInPlace ? (*policy.ReloadInPlace)(data, metadata) : RefreshResult::NotLoaded;
			if (refreshed == RefreshResult::NotLoaded)
			{
				// Nothing to reload in place: a shader that failed its first load was
				// destroyed rather than kept as an invalid object.
				LoadInternal(data, metadata);
			}
		}
	}

	AssetManager::AssetManager(const AssetManagerParams& params, AudioEngine* audioEngine)
		: m_Data(std::make_unique<AssetManagerData>())
	{
		m_Data->Params = params;
		m_Data->Audio = audioEngine;
	}

	AssetManager::~AssetManager()
	{
		Shutdown();
	}

	void AssetManager::Initialize()
	{
		AssetManagerData& data = *m_Data;

		// lexically_normal keeps a trailing separator ("assets/" stays "assets/"), which would
		// push every prefix test against the root off by one character and silently defeat the
		// absolute-to-relative folding in NormalizeRelativePath. Strip it once, here, so every
		// consumer of RootDirectory sees a single spelling.
		std::filesystem::path root = std::filesystem::absolute(data.Params.RootDirectory).lexically_normal();
		if (!root.has_filename() && root.parent_path() != root)
			root = root.parent_path();
		data.RootDirectory = std::move(root);

		if (!std::filesystem::exists(data.RootDirectory))
			DE_CORE_WARN("AssetManager: asset root '{}' does not exist - loads will fail until it does.", data.RootDirectory.string());
		else
			DE_CORE_INFO("AssetManager: asset root '{}'.", data.RootDirectory.string());

		data.Worker = std::thread(WorkerLoop, std::ref(data));
	}

	void AssetManager::Shutdown()
	{
		AssetManagerData& data = *m_Data;

		if (data.Worker.joinable())
		{
			{
				std::scoped_lock lock(data.JobMutex);
				data.StopWorker = true;
				data.Jobs.clear();
			}
			data.JobCV.notify_one();
			data.Worker.join();
			data.StopWorker = false;
		}

		// Free payloads that completed but were never finalized.
		for (AsyncResult& result : data.Results)
		{
			if (result.Pixels)
				FileSystem::FreeImage(result.Pixels);
		}
		data.Results.clear();
		data.MainThreadQueue.clear();
		data.PendingCount = 0;
		data.ReloadCount = 0;

		Utils::DestroyAll(data.Textures);
		Utils::DestroyAll(data.Shaders);
		Utils::DestroyAll(data.Models);
		Utils::DestroyAll(data.Fonts);
		data.AudioClips.clear();

		data.Registry.clear();
		data.PathLookup.clear();
	}

	AssetHandle AssetManager::Import(const std::filesystem::path& path)
	{
		AssetManagerData& data = *m_Data;
		const std::string key = NormalizePathKey(data, path);

		auto existing = data.PathLookup.find(key);
		if (existing != data.PathLookup.end())
			return existing->second;

		const AssetType type = AssetTypeFromExtension(path.extension());
		if (type == AssetType::None)
		{
			DE_CORE_ERROR("AssetManager: cannot import '{}' - unrecognized extension '{}'.", path.string(), path.extension().string());
			return k_InvalidAsset;
		}

		AssetMetadata metadata;
		metadata.Handle = AssetHandle();
		// A UUID collision is astronomically unlikely, but it would silently overwrite a live
		// registration - and re-rolling costs one lookup, once, at import time.
		while (!IsValidAssetHandle(metadata.Handle) || data.Registry.contains(metadata.Handle))
			metadata.Handle = AssetHandle();

		metadata.Type = type;
		metadata.FilePath = NormalizeRelativePath(data, path);
		metadata.AbsolutePath = ResolvePath(metadata.FilePath);
		metadata.State = AssetState::Unloaded;

		const AssetHandle handle = metadata.Handle;
		data.Registry[handle] = std::move(metadata);
		data.PathLookup[key] = handle;

		return handle;
	}

	AssetHandle AssetManager::Load(const std::filesystem::path& path)
	{
		const AssetHandle handle = Import(path);
		if (!IsValidAssetHandle(handle))
			return k_InvalidAsset;

		AssetMetadata& metadata = m_Data->Registry.at(handle);
		// Unloaded/Failed only: an asset already Ready needs nothing, and one in
		// flight on the loader thread (Queued/Loading/Reloading) will publish via Update().
		if (metadata.State == AssetState::Unloaded || metadata.State == AssetState::Failed)
			LoadInternal(*m_Data, metadata);

		return handle;
	}

	AssetHandle AssetManager::LoadAsync(const std::filesystem::path& path)
	{
		const AssetHandle handle = Import(path);
		if (!IsValidAssetHandle(handle))
			return k_InvalidAsset;

		AssetManagerData& data = *m_Data;
		AssetMetadata& metadata = data.Registry.at(handle);
		if (metadata.State != AssetState::Unloaded && metadata.State != AssetState::Failed)
			return handle;

		if (PolicyFor(metadata.Type).Decode)
		{
			// Stamp at queue time, not at finalize: an edit landing while the decode is
			// in flight then still differs from the stamp and the next poll catches it.
			Utils::StampWriteTime(metadata, metadata.AbsolutePath);
			metadata.State = AssetState::Loading;
			++data.PendingCount;
			QueueDecodeJob(data, metadata, false);
		}
		else
		{
			metadata.State = AssetState::Queued;
			++data.PendingCount;
			data.MainThreadQueue.push_back(handle);
		}

		return handle;
	}

	bool AssetManager::Reload(AssetHandle handle)
	{
		AssetManagerData& data = *m_Data;
		auto it = data.Registry.find(handle);
		if (it == data.Registry.end())
		{
			DE_CORE_WARN("AssetManager: Reload on unknown handle {}.", static_cast<uint64_t>(handle));
			return false;
		}

		AssetMetadata& metadata = it->second;

		// A load is already in flight. Unloading and loading synchronously here would leave
		// the in-flight request unwanted, so its finished decode would be thrown away - and
		// it publishes the file as it stands on disk anyway, which is what a reload wants.
		if (metadata.State == AssetState::Queued || metadata.State == AssetState::Loading || metadata.State == AssetState::Reloading)
		{
			DE_CORE_WARN("AssetManager: Reload ignored for '{}' - a load is already in flight.", metadata.FilePath.generic_string());
			return IsReady(handle);
		}

		// Prefer the in-place path so a reload behaves like hot-reload does: the loaded
		// object is refreshed rather than replaced, and pointers already handed out stay
		// valid. Only the destroy-and-recreate fallback below invalidates them.
		if (metadata.State == AssetState::Ready && ReloadInPlace(data, metadata))
			return true;

		UnloadInternal(data, metadata);
		return LoadInternal(data, metadata);
	}

	bool AssetManager::SupportsInPlaceReload(AssetType type)
	{
		return PolicyFor(type).ReloadInPlace != nullptr;
	}

	void AssetManager::Unload(AssetHandle handle)
	{
		auto it = m_Data->Registry.find(handle);
		if (it == m_Data->Registry.end())
			return;

		UnloadInternal(*m_Data, it->second);
	}

	void AssetManager::Remove(AssetHandle handle)
	{
		AssetManagerData& data = *m_Data;
		auto it = data.Registry.find(handle);
		if (it == data.Registry.end())
			return;

		UnloadInternal(data, it->second);
		data.PathLookup.erase(NormalizePathKey(data, it->second.FilePath));
		data.Registry.erase(it);
	}

	Texture* AssetManager::GetTexture(AssetHandle handle) const
	{
		auto it = m_Data->Textures.find(handle);
		return it != m_Data->Textures.end() ? it->second : nullptr;
	}

	Shader* AssetManager::GetShader(AssetHandle handle) const
	{
		auto it = m_Data->Shaders.find(handle);
		return it != m_Data->Shaders.end() ? it->second : nullptr;
	}

	Model* AssetManager::GetModel(AssetHandle handle) const
	{
		auto it = m_Data->Models.find(handle);
		return it != m_Data->Models.end() ? it->second : nullptr;
	}

	Font* AssetManager::GetFont(AssetHandle handle) const
	{
		auto it = m_Data->Fonts.find(handle);
		return it != m_Data->Fonts.end() ? it->second : nullptr;
	}

	std::shared_ptr<AudioClip> AssetManager::GetAudioClip(AssetHandle handle) const
	{
		auto it = m_Data->AudioClips.find(handle);
		return it != m_Data->AudioClips.end() ? it->second : nullptr;
	}

	AssetState AssetManager::GetState(AssetHandle handle) const
	{
		auto it = m_Data->Registry.find(handle);
		return it != m_Data->Registry.end() ? it->second.State : AssetState::Unloaded;
	}

	const AssetMetadata* AssetManager::GetMetadata(AssetHandle handle) const
	{
		auto it = m_Data->Registry.find(handle);
		return it != m_Data->Registry.end() ? &it->second : nullptr;
	}

	AssetHandle AssetManager::FindByPath(const std::filesystem::path& path) const
	{
		auto it = m_Data->PathLookup.find(NormalizePathKey(*m_Data, path));
		return it != m_Data->PathLookup.end() ? it->second : k_InvalidAsset;
	}

	const std::filesystem::path& AssetManager::GetRootDirectory() const
	{
		return m_Data->RootDirectory;
	}

	std::filesystem::path AssetManager::ResolvePath(const std::filesystem::path& path) const
	{
		// operator/ discards the left side for absolute right-hand paths, so absolute
		// asset paths pass through unchanged.
		return (m_Data->RootDirectory / path).lexically_normal();
	}

	const std::unordered_map<AssetHandle, AssetMetadata>& AssetManager::GetRegistry() const
	{
		return m_Data->Registry;
	}

	bool AssetManager::IsHotReloadEnabled() const
	{
		return m_Data->Params.EnableHotReload;
	}

	void AssetManager::SetHotReloadEnabled(bool enable)
	{
		AssetManagerData& data = *m_Data;
		if (data.Params.EnableHotReload == enable)
			return;

		data.Params.EnableHotReload = enable;
		data.HotReloadTimer = 0.0f;

		if (!enable)
			return;

		// Re-stamp what is loaded: edits made while watching was off must not be
		// reported as changes the moment it is switched back on.
		for (auto& [handle, metadata] : data.Registry)
		{
			if (metadata.State == AssetState::Ready)
				Utils::StampWriteTime(metadata, metadata.AbsolutePath);
		}
	}

	uint32_t AssetManager::GetRegisteredCount() const
	{
		return static_cast<uint32_t>(m_Data->Registry.size());
	}

	uint32_t AssetManager::GetLoadedCount() const
	{
		const AssetManagerData& data = *m_Data;
		return static_cast<uint32_t>(data.Textures.size() + data.Shaders.size() + data.Models.size() + data.Fonts.size() + data.AudioClips.size());
	}

	uint32_t AssetManager::GetPendingCount() const
	{
		return m_Data->PendingCount.load();
	}

	uint32_t AssetManager::GetReloadingCount() const
	{
		return m_Data->ReloadCount.load();
	}

	void AssetManager::Update(float deltaTime)
	{
		AssetManagerData& data = *m_Data;

		if (data.Params.EnableHotReload)
		{
			data.HotReloadTimer += deltaTime;
			if (data.HotReloadTimer >= data.Params.HotReloadInterval)
			{
				data.HotReloadTimer = 0.0f;
				PollHotReload(data);
			}
		}

		// One main-thread fallback load per frame keeps loading-screen frames responsive.
		if (!data.MainThreadQueue.empty())
		{
			const AssetHandle handle = data.MainThreadQueue.front();
			data.MainThreadQueue.pop_front();

			auto it = data.Registry.find(handle);
			// Anything but Queued means the request was satisfied or cancelled meanwhile.
			if (it != data.Registry.end() && it->second.State == AssetState::Queued)
				LoadInternal(data, it->second);
			--data.PendingCount;
		}

		std::vector<AsyncResult> results;
		{
			std::scoped_lock lock(data.ResultMutex);
			results.swap(data.Results);
		}
		for (AsyncResult& result : results)
			FinalizeResult(data, result);
	}

}
