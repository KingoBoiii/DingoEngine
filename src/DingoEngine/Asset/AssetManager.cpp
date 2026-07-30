#include "depch.h"
#include "DingoEngine/Asset/AssetManager.h"

#include "DingoEngine/Core/FileSystem.h"
#include "DingoEngine/Graphics/Texture.h"
#include "DingoEngine/Graphics/Shader.h"
#include "DingoEngine/Graphics/Model.h"
#include "DingoEngine/Graphics/Font.h"
#include "DingoEngine/Audio/AudioEngine.h"

namespace Dingo
{

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
	struct AssetManager::AssetTypePolicy
	{
		AssetType Type = AssetType::None;

		// Creates the object and stores it; the caller owns stamping and the state flip.
		bool (*Load)(AssetManager&, const AssetMetadata&) = nullptr;
		// Refreshes the loaded object's contents without replacing it, so pointers already
		// handed out stay valid. Null for types that can only be destroyed and recreated.
		RefreshResult (*ReloadInPlace)(AssetManager&, const AssetMetadata&) = nullptr;
		void (*Unload)(AssetManager&, AssetHandle) = nullptr;
		bool (*IsLoaded)(const AssetManager&, AssetHandle) = nullptr;
		// Decode runs on the loader thread and must touch nothing on the manager beyond
		// the audio engine pointer, which is fixed for its lifetime - all registry and
		// asset-map state stays main-thread-owned. Publish applies the payload in the
		// main-thread pump. Both null for types that load synchronously.
		void (*Decode)(AssetManager&, const AsyncJob&, AsyncResult&) = nullptr;
		void (*Publish)(AssetManager&, AsyncResult&) = nullptr;

		bool HotReloadWatched = false;
	};

	const AssetManager::AssetTypePolicy& AssetManager::PolicyFor(AssetType type)
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
				.Load = [](AssetManager& self, const AssetMetadata& metadata) -> bool
				{
					Texture* texture = Texture::CreateFromFile(metadata.AbsolutePath, metadata.FilePath.generic_string());
					if (!texture)
						return false;

					self.m_Textures[metadata.Handle] = texture;
					return true;
				},
				.ReloadInPlace = [](AssetManager& self, const AssetMetadata& metadata) -> RefreshResult
				{
					auto it = self.m_Textures.find(metadata.Handle);
					if (it == self.m_Textures.end())
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
				.Unload = [](AssetManager& self, AssetHandle handle) { Utils::DestroyFrom(self.m_Textures, handle); },
				.IsLoaded = [](const AssetManager& self, AssetHandle handle) -> bool { return self.m_Textures.contains(handle); },
				.Decode = [](AssetManager&, const AsyncJob& job, AsyncResult& result)
				{
					uint32_t width = 0, height = 0, channels = 0;
					result.Pixels = FileSystem::ReadImage(job.AbsolutePath, &width, &height, &channels, true, true);
					result.Width = width;
					result.Height = height;
					result.Channels = channels;
					result.Success = result.Pixels != nullptr;
				},
				.Publish = [](AssetManager& self, AsyncResult& result)
				{
					const TextureParams textureParams = Utils::MakeTextureParams(result.DebugName, result.Width, result.Height, result.Channels, result.Pixels);

					auto existing = self.m_Textures.find(result.Handle);
					if (existing != self.m_Textures.end())
					{
						// Hot-reload: swap the contents inside the same object so every
						// Texture* held by game code keeps working.
						existing->second->Reinitialize(textureParams);
					}
					else
					{
						self.m_Textures[result.Handle] = Texture::Create(textureParams);
					}
				},
				.HotReloadWatched = true
			},
			{
				.Type = AssetType::Shader,
				.Load = [](AssetManager& self, const AssetMetadata& metadata) -> bool
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

					self.m_Shaders[metadata.Handle] = shader;
					return true;
				},
				.ReloadInPlace = [](AssetManager& self, const AssetMetadata& metadata) -> RefreshResult
				{
					auto it = self.m_Shaders.find(metadata.Handle);
					if (it == self.m_Shaders.end())
						return RefreshResult::NotLoaded;

					it->second->Reload(); // keeps the previous program on a compile error
					return RefreshResult::Refreshed;
				},
				.Unload = [](AssetManager& self, AssetHandle handle) { Utils::DestroyFrom(self.m_Shaders, handle); },
				.IsLoaded = [](const AssetManager& self, AssetHandle handle) -> bool { return self.m_Shaders.contains(handle); },
				.HotReloadWatched = true
			},
			{
				.Type = AssetType::Model,
				.Load = [](AssetManager& self, const AssetMetadata& metadata) -> bool
				{
					Model* model = Model::LoadFromFile(metadata.AbsolutePath);
					if (!model)
						return false;

					self.m_Models[metadata.Handle] = model;
					return true;
				},
				.Unload = [](AssetManager& self, AssetHandle handle) { Utils::DestroyFrom(self.m_Models, handle); },
				.IsLoaded = [](const AssetManager& self, AssetHandle handle) -> bool { return self.m_Models.contains(handle); }
			},
			{
				.Type = AssetType::Font,
				.Load = [](AssetManager& self, const AssetMetadata& metadata) -> bool
				{
					FontParams fontParams;
					fontParams.Name = Utils::SanitizeAssetName(metadata.FilePath);

					Font* font = Font::Create(metadata.AbsolutePath, fontParams);
					if (!font)
						return false;

					self.m_Fonts[metadata.Handle] = font;
					return true;
				},
				.Unload = [](AssetManager& self, AssetHandle handle) { Utils::DestroyFrom(self.m_Fonts, handle); },
				.IsLoaded = [](const AssetManager& self, AssetHandle handle) -> bool { return self.m_Fonts.contains(handle); }
			},
			{
				.Type = AssetType::AudioClip,
				.Load = [](AssetManager& self, const AssetMetadata& metadata) -> bool
				{
					DE_CORE_ASSERT(self.m_AudioEngine, "AssetManager has no audio engine - cannot load audio clips");
					std::shared_ptr<AudioClip> clip = self.m_AudioEngine->LoadClip(metadata.AbsolutePath);
					if (!clip)
						return false;

					self.m_AudioClips[metadata.Handle] = std::move(clip);
					return true;
				},
				.Unload = [](AssetManager& self, AssetHandle handle) { self.m_AudioClips.erase(handle); },
				.IsLoaded = [](const AssetManager& self, AssetHandle handle) -> bool { return self.m_AudioClips.contains(handle); },
				.Decode = [](AssetManager& self, const AsyncJob& job, AsyncResult& result)
				{
					result.Clip = self.m_AudioEngine->LoadClip(job.AbsolutePath);
					result.Success = result.Clip != nullptr;
				},
				.Publish = [](AssetManager& self, AsyncResult& result)
				{
					self.m_AudioClips[result.Handle] = std::move(result.Clip);
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

	AssetManager::AssetManager(const AssetManagerParams& params, AudioEngine* audioEngine)
		: m_Params(params), m_AudioEngine(audioEngine)
	{}

	AssetManager::~AssetManager()
	{
		Shutdown();
	}

	void AssetManager::Initialize()
	{
		m_RootDirectory = std::filesystem::absolute(m_Params.RootDirectory).lexically_normal();

		if (!std::filesystem::exists(m_RootDirectory))
			DE_CORE_WARN("AssetManager: asset root '{}' does not exist - loads will fail until it does.", m_RootDirectory.string());
		else
			DE_CORE_INFO("AssetManager: asset root '{}'.", m_RootDirectory.string());

		m_Worker = std::thread(&AssetManager::WorkerLoop, this);
	}

	void AssetManager::Shutdown()
	{
		if (m_Worker.joinable())
		{
			{
				std::scoped_lock lock(m_JobMutex);
				m_StopWorker = true;
				m_Jobs.clear();
			}
			m_JobCV.notify_one();
			m_Worker.join();
			m_StopWorker = false;
		}

		// Free payloads that completed but were never finalized.
		for (AsyncResult& result : m_Results)
		{
			if (result.Pixels)
				FileSystem::FreeImage(result.Pixels);
		}
		m_Results.clear();
		m_MainThreadQueue.clear();
		m_PendingCount = 0;

		Utils::DestroyAll(m_Textures);
		Utils::DestroyAll(m_Shaders);
		Utils::DestroyAll(m_Models);
		Utils::DestroyAll(m_Fonts);
		m_AudioClips.clear();

		m_Registry.clear();
		m_PathLookup.clear();
	}

	AssetHandle AssetManager::Import(const std::filesystem::path& path)
	{
		const std::string key = NormalizePathKey(path);

		auto existing = m_PathLookup.find(key);
		if (existing != m_PathLookup.end())
			return existing->second;

		const AssetType type = AssetTypeFromExtension(path.extension());
		if (type == AssetType::None)
		{
			DE_CORE_ERROR("AssetManager: cannot import '{}' - unrecognized extension '{}'.", path.string(), path.extension().string());
			return k_InvalidAsset;
		}

		AssetMetadata metadata;
		metadata.Handle = AssetHandle();
		metadata.Type = type;
		metadata.FilePath = std::filesystem::path(key);
		metadata.AbsolutePath = ResolvePath(metadata.FilePath);
		metadata.State = AssetState::Unloaded;

		const AssetHandle handle = metadata.Handle;
		m_Registry[handle] = std::move(metadata);
		m_PathLookup[key] = handle;

		return handle;
	}

	AssetHandle AssetManager::Load(const std::filesystem::path& path)
	{
		const AssetHandle handle = Import(path);
		if (!IsValidAssetHandle(handle))
			return k_InvalidAsset;

		AssetMetadata& metadata = m_Registry.at(handle);
		// Unloaded/Failed only: an asset already Ready needs nothing, and one in
		// flight on the loader thread (Queued/Loading) will publish via Update().
		if (metadata.State == AssetState::Unloaded || metadata.State == AssetState::Failed)
			LoadInternal(metadata);

		return handle;
	}

	AssetHandle AssetManager::LoadAsync(const std::filesystem::path& path)
	{
		const AssetHandle handle = Import(path);
		if (!IsValidAssetHandle(handle))
			return k_InvalidAsset;

		AssetMetadata& metadata = m_Registry.at(handle);
		if (metadata.State != AssetState::Unloaded && metadata.State != AssetState::Failed)
			return handle;

		if (PolicyFor(metadata.Type).Decode)
		{
			// Stamp at queue time, not at finalize: an edit landing while the decode is
			// in flight then still differs from the stamp and the next poll catches it.
			Utils::StampWriteTime(metadata, metadata.AbsolutePath);
			metadata.State = AssetState::Loading;
			++m_PendingCount;
			QueueDecodeJob(metadata);
		}
		else
		{
			metadata.State = AssetState::Queued;
			++m_PendingCount;
			m_MainThreadQueue.push_back(handle);
		}

		return handle;
	}

	bool AssetManager::Reload(AssetHandle handle)
	{
		auto it = m_Registry.find(handle);
		if (it == m_Registry.end())
		{
			DE_CORE_WARN("AssetManager: Reload on unknown handle {}.", static_cast<uint64_t>(handle));
			return false;
		}

		AssetMetadata& metadata = it->second;

		// Prefer the in-place path so a reload behaves like hot-reload does: the loaded
		// object is refreshed rather than replaced, and pointers already handed out stay
		// valid. Only the destroy-and-recreate fallback below invalidates them.
		if (metadata.State == AssetState::Ready && ReloadInPlace(metadata))
			return true;

		UnloadInternal(metadata);
		return LoadInternal(metadata);
	}

	bool AssetManager::SupportsInPlaceReload(AssetType type)
	{
		return PolicyFor(type).ReloadInPlace != nullptr;
	}

	bool AssetManager::ReloadInPlace(AssetMetadata& metadata)
	{
		const AssetTypePolicy& policy = PolicyFor(metadata.Type);
		if (!policy.ReloadInPlace)
			return false;

		const RefreshResult result = (*policy.ReloadInPlace)(*this, metadata);
		if (result == RefreshResult::NotLoaded)
			return false;

		if (result == RefreshResult::Refreshed)
			Utils::StampWriteTime(metadata, metadata.AbsolutePath);

		return true;
	}

	void AssetManager::Unload(AssetHandle handle)
	{
		auto it = m_Registry.find(handle);
		if (it == m_Registry.end())
			return;

		UnloadInternal(it->second);
	}

	void AssetManager::Remove(AssetHandle handle)
	{
		auto it = m_Registry.find(handle);
		if (it == m_Registry.end())
			return;

		UnloadInternal(it->second);
		m_PathLookup.erase(it->second.FilePath.generic_string());
		m_Registry.erase(it);
	}

	Texture* AssetManager::GetTexture(AssetHandle handle) const
	{
		auto it = m_Textures.find(handle);
		return it != m_Textures.end() ? it->second : nullptr;
	}

	Shader* AssetManager::GetShader(AssetHandle handle) const
	{
		auto it = m_Shaders.find(handle);
		return it != m_Shaders.end() ? it->second : nullptr;
	}

	Model* AssetManager::GetModel(AssetHandle handle) const
	{
		auto it = m_Models.find(handle);
		return it != m_Models.end() ? it->second : nullptr;
	}

	Font* AssetManager::GetFont(AssetHandle handle) const
	{
		auto it = m_Fonts.find(handle);
		return it != m_Fonts.end() ? it->second : nullptr;
	}

	std::shared_ptr<AudioClip> AssetManager::GetAudioClip(AssetHandle handle) const
	{
		auto it = m_AudioClips.find(handle);
		return it != m_AudioClips.end() ? it->second : nullptr;
	}

	AssetState AssetManager::GetState(AssetHandle handle) const
	{
		auto it = m_Registry.find(handle);
		return it != m_Registry.end() ? it->second.State : AssetState::Unloaded;
	}

	const AssetMetadata* AssetManager::GetMetadata(AssetHandle handle) const
	{
		auto it = m_Registry.find(handle);
		return it != m_Registry.end() ? &it->second : nullptr;
	}

	AssetHandle AssetManager::FindByPath(const std::filesystem::path& path) const
	{
		auto it = m_PathLookup.find(NormalizePathKey(path));
		return it != m_PathLookup.end() ? it->second : k_InvalidAsset;
	}

	std::filesystem::path AssetManager::ResolvePath(const std::filesystem::path& path) const
	{
		// operator/ discards the left side for absolute right-hand paths, so absolute
		// asset paths pass through unchanged.
		return (m_RootDirectory / path).lexically_normal();
	}

	void AssetManager::SetHotReloadEnabled(bool enable)
	{
		if (m_Params.EnableHotReload == enable)
			return;

		m_Params.EnableHotReload = enable;
		m_HotReloadTimer = 0.0f;

		if (!enable)
			return;

		// Re-stamp what is loaded: edits made while watching was off must not be
		// reported as changes the moment it is switched back on.
		for (auto& [handle, metadata] : m_Registry)
		{
			if (metadata.State == AssetState::Ready)
				Utils::StampWriteTime(metadata, metadata.AbsolutePath);
		}
	}

	uint32_t AssetManager::GetLoadedCount() const
	{
		return static_cast<uint32_t>(m_Textures.size() + m_Shaders.size() + m_Models.size() + m_Fonts.size() + m_AudioClips.size());
	}

	void AssetManager::Update(float deltaTime)
	{
		if (m_Params.EnableHotReload)
		{
			m_HotReloadTimer += deltaTime;
			if (m_HotReloadTimer >= m_Params.HotReloadInterval)
			{
				m_HotReloadTimer = 0.0f;
				PollHotReload();
			}
		}

		// One main-thread fallback load per frame keeps loading-screen frames responsive.
		if (!m_MainThreadQueue.empty())
		{
			const AssetHandle handle = m_MainThreadQueue.front();
			m_MainThreadQueue.pop_front();

			auto it = m_Registry.find(handle);
			// Anything but Queued means the request was satisfied or cancelled meanwhile.
			if (it != m_Registry.end() && it->second.State == AssetState::Queued)
				LoadInternal(it->second);
			--m_PendingCount;
		}

		std::vector<AsyncResult> results;
		{
			std::scoped_lock lock(m_ResultMutex);
			results.swap(m_Results);
		}
		for (AsyncResult& result : results)
			FinalizeResult(result);
	}

	void AssetManager::WorkerLoop()
	{
		for (;;)
		{
			AsyncJob job;
			{
				std::unique_lock lock(m_JobMutex);
				m_JobCV.wait(lock, [this] { return m_StopWorker || !m_Jobs.empty(); });
				if (m_StopWorker)
					return;

				job = std::move(m_Jobs.front());
				m_Jobs.pop_front();
			}

			AsyncResult result;
			result.Handle = job.Handle;
			result.Type = job.Type;
			result.DebugName = std::move(job.DebugName);

			// An exception escaping a thread function is a hard process kill, with no log line
			// and no chance to mark the asset Failed. The decoders reach std::filesystem
			// (miniaudio uses the throwing `exists` overload), which throws for a path the OS
			// rejects — a bad character or one over MAX_PATH. Route it back as a normal failure.
			try
			{
				if (const AssetTypePolicy& policy = PolicyFor(job.Type); policy.Decode)
					(*policy.Decode)(*this, job, result);
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
				std::scoped_lock lock(m_ResultMutex);
				m_Results.push_back(std::move(result));
			}
		}
	}

	void AssetManager::FinalizeResult(AsyncResult& result)
	{
		auto it = m_Registry.find(result.Handle);
		// Only publish if the request is still wanted - Unload/Remove/Reload during the
		// background decode leaves the state != Loading and the payload is discarded.
		const bool stillWanted = it != m_Registry.end() && it->second.State == AssetState::Loading;
		const AssetTypePolicy& policy = PolicyFor(result.Type);

		if (stillWanted && result.Success)
		{
			if (policy.Publish)
			{
				(*policy.Publish)(*this, result);
				it->second.State = AssetState::Ready;
			}
		}
		else if (stillWanted)
		{
			DE_CORE_ERROR("AssetManager: async load failed for {} '{}'.", AssetTypeToString(result.Type), result.DebugName);
			// A failed hot-reload keeps serving the still-loaded previous version.
			const bool hasLoadedObject = policy.IsLoaded && (*policy.IsLoaded)(*this, result.Handle);
			it->second.State = hasLoadedObject ? AssetState::Ready : AssetState::Failed;
		}

		if (result.Pixels)
			FileSystem::FreeImage(result.Pixels);
		--m_PendingCount;
	}

	void AssetManager::QueueDecodeJob(const AssetMetadata& metadata)
	{
		AsyncJob job;
		job.Handle = metadata.Handle;
		job.Type = metadata.Type;
		job.AbsolutePath = metadata.AbsolutePath;
		job.DebugName = metadata.FilePath.generic_string();
		{
			std::scoped_lock lock(m_JobMutex);
			m_Jobs.push_back(std::move(job));
		}
		m_JobCV.notify_one();
	}

	void AssetManager::PollHotReload()
	{
		// Collect the watchable handles once per pass instead of walking the whole registry
		// every tick. Handles are stable, so an entry removed mid-pass just misses its turn.
		if (m_WatchCursor >= m_WatchList.size())
		{
			m_WatchList.clear();
			for (const auto& [handle, metadata] : m_Registry)
			{
				if (PolicyFor(metadata.Type).HotReloadWatched)
					m_WatchList.push_back(handle);
			}
			m_WatchCursor = 0;
		}

		// Cap the stat() calls a single poll can make. A project with fewer watched assets
		// than the cap still gets its whole list checked every tick, so nothing about
		// detection latency changes there; only projects big enough for the syscalls to cost
		// real frame time spread the work out, and they spread it in proportion. This also
		// bounds the cost when HotReloadInterval is 0, which polls every frame.
		const std::size_t passEnd = std::min(m_WatchList.size(), m_WatchCursor + k_MaxWatchChecksPerPoll);

		for (; m_WatchCursor < passEnd; ++m_WatchCursor)
		{
			auto entry = m_Registry.find(m_WatchList[m_WatchCursor]);
			if (entry == m_Registry.end())
				continue;

			AssetMetadata& metadata = entry->second;

			// Failed is watched as well as Ready: a first-load failure keeps its
			// registration precisely so fixing the file recovers it. Its write time was
			// never stamped, so the first poll fires immediately - and a file that is still
			// broken gets one attempt per edit, not one per poll, because the stamp below
			// lands whether or not the reload succeeds.
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
				metadata.State = AssetState::Loading;
				++m_PendingCount;
				QueueDecodeJob(metadata);
				continue;
			}

			// The stamp above is already committed, so the refresh must not stamp again.
			const RefreshResult refreshed = policy.ReloadInPlace ? (*policy.ReloadInPlace)(*this, metadata) : RefreshResult::NotLoaded;
			if (refreshed == RefreshResult::NotLoaded)
			{
				// Nothing to reload in place: a shader that failed its first load was
				// destroyed rather than kept as an invalid object.
				LoadInternal(metadata);
			}
		}
	}

	bool AssetManager::LoadInternal(AssetMetadata& metadata)
	{
		const AssetTypePolicy& policy = PolicyFor(metadata.Type);
		if (policy.Load && (*policy.Load)(*this, metadata))
		{
			Utils::StampWriteTime(metadata, metadata.AbsolutePath);
			metadata.State = AssetState::Ready;
			return true;
		}

		DE_CORE_ERROR("AssetManager: failed to load {} '{}'.", AssetTypeToString(metadata.Type), metadata.AbsolutePath.string());
		metadata.State = AssetState::Failed;
		return false;
	}

	void AssetManager::UnloadInternal(const AssetMetadata& metadata)
	{
		const AssetTypePolicy& policy = PolicyFor(metadata.Type);
		if (policy.Unload)
			(*policy.Unload)(*this, metadata.Handle);

		m_Registry.at(metadata.Handle).State = AssetState::Unloaded;
	}

	std::string AssetManager::NormalizePathKey(const std::filesystem::path& path)
	{
		return path.lexically_normal().generic_string();
	}

}
