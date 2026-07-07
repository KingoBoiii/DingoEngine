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

		for (auto& [handle, texture] : m_Textures)
		{
			texture->Destroy();
			delete texture;
		}
		m_Textures.clear();

		for (auto& [handle, shader] : m_Shaders)
		{
			shader->Destroy();
			delete shader;
		}
		m_Shaders.clear();

		for (auto& [handle, model] : m_Models)
		{
			model->Destroy();
			delete model;
		}
		m_Models.clear();

		for (auto& [handle, font] : m_Fonts)
		{
			font->Destroy();
			delete font;
		}
		m_Fonts.clear();

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

		if (metadata.Type == AssetType::Texture2D || metadata.Type == AssetType::AudioClip)
		{
			// Stamp at queue time, not at finalize: an edit landing while the decode is
			// in flight then still differs from the stamp and the next poll catches it.
			Utils::StampWriteTime(metadata, ResolvePath(metadata.FilePath));
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

		UnloadInternal(it->second);
		return LoadInternal(it->second);
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

			switch (job.Type)
			{
				case AssetType::Texture2D:
				{
					uint32_t width = 0, height = 0, channels = 0;
					result.Pixels = FileSystem::ReadImage(job.AbsolutePath, &width, &height, &channels, true, true);
					result.Width = width;
					result.Height = height;
					result.Success = result.Pixels != nullptr;
					break;
				}
				case AssetType::AudioClip:
				{
					std::scoped_lock audioLock(m_AudioLoadMutex);
					result.Clip = m_AudioEngine->LoadClip(job.AbsolutePath);
					result.Success = result.Clip != nullptr;
					break;
				}
				default:
					break;
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

		if (stillWanted && result.Success)
		{
			switch (result.Type)
			{
				case AssetType::Texture2D:
				{
					const TextureParams textureParams = TextureParams()
						.SetDebugName(result.DebugName)
						.SetWidth(result.Width)
						.SetHeight(result.Height)
						.SetDimension(TextureDimension::Texture2D)
						.SetFormat(TextureFormat::RGBA)
						.SetIsRenderTarget(false)
						.SetInitialData(result.Pixels);

					auto existing = m_Textures.find(result.Handle);
					if (existing != m_Textures.end())
					{
						// Hot-reload: swap the contents inside the same object so every
						// Texture* held by game code keeps working.
						existing->second->Reinitialize(textureParams);
					}
					else
					{
						m_Textures[result.Handle] = Texture::Create(textureParams);
					}
					it->second.State = AssetState::Ready;
					break;
				}
				case AssetType::AudioClip:
				{
					m_AudioClips[result.Handle] = std::move(result.Clip);
					it->second.State = AssetState::Ready;
					break;
				}
				default:
					break;
			}
		}
		else if (stillWanted)
		{
			DE_CORE_ERROR("AssetManager: async load failed for {} '{}'.", AssetTypeToString(result.Type), result.DebugName);
			// A failed hot-reload keeps serving the still-loaded previous version.
			const bool hasLoadedObject = m_Textures.contains(result.Handle) || m_AudioClips.contains(result.Handle);
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
		job.AbsolutePath = ResolvePath(metadata.FilePath);
		job.DebugName = metadata.FilePath.generic_string();
		{
			std::scoped_lock lock(m_JobMutex);
			m_Jobs.push_back(std::move(job));
		}
		m_JobCV.notify_one();
	}

	void AssetManager::PollHotReload()
	{
		for (auto& [handle, metadata] : m_Registry)
		{
			if (metadata.State != AssetState::Ready)
				continue;
			if (metadata.Type != AssetType::Texture2D && metadata.Type != AssetType::Shader)
				continue;

			std::error_code ec;
			const auto writeTime = std::filesystem::last_write_time(ResolvePath(metadata.FilePath), ec);
			if (ec || writeTime == metadata.LastWriteTime)
				continue;

			metadata.LastWriteTime = writeTime;
			DE_CORE_INFO("AssetManager: '{}' changed on disk - hot-reloading.", metadata.FilePath.generic_string());

			if (metadata.Type == AssetType::Texture2D)
			{
				metadata.State = AssetState::Loading;
				++m_PendingCount;
				QueueDecodeJob(metadata);
			}
			else
			{
				auto it = m_Shaders.find(handle);
				if (it != m_Shaders.end())
					it->second->Reload(); // keeps the previous program on compile failure
			}
		}
	}

	bool AssetManager::LoadInternal(AssetMetadata& metadata)
	{
		const std::filesystem::path absolutePath = ResolvePath(metadata.FilePath);
		const std::string name = Utils::SanitizeAssetName(metadata.FilePath);

		switch (metadata.Type)
		{
			case AssetType::Texture2D:
			{
				if (Texture* texture = Texture::CreateFromFile(absolutePath, metadata.FilePath.generic_string()))
				{
					m_Textures[metadata.Handle] = texture;
					Utils::StampWriteTime(metadata, absolutePath);
					metadata.State = AssetState::Ready;
					return true;
				}
				break;
			}
			case AssetType::Shader:
			{
				// Shader::Create never returns nullptr - a failed compile/read yields an
				// object with no program, detected via IsValid().
				Shader* shader = Shader::CreateFromFile(name, absolutePath);
				if (shader && !shader->IsValid())
				{
					shader->Destroy();
					delete shader;
					shader = nullptr;
				}
				if (shader)
				{
					m_Shaders[metadata.Handle] = shader;
					Utils::StampWriteTime(metadata, absolutePath);
					metadata.State = AssetState::Ready;
					return true;
				}
				break;
			}
			case AssetType::Model:
			{
				if (Model* model = Model::LoadFromFile(absolutePath))
				{
					m_Models[metadata.Handle] = model;
					Utils::StampWriteTime(metadata, absolutePath);
					metadata.State = AssetState::Ready;
					return true;
				}
				break;
			}
			case AssetType::Font:
			{
				FontParams fontParams;
				fontParams.Name = name;
				if (Font* font = Font::Create(absolutePath, fontParams))
				{
					m_Fonts[metadata.Handle] = font;
					Utils::StampWriteTime(metadata, absolutePath);
					metadata.State = AssetState::Ready;
					return true;
				}
				break;
			}
			case AssetType::AudioClip:
			{
				DE_CORE_ASSERT(m_AudioEngine, "AssetManager has no audio engine - cannot load audio clips");
				std::scoped_lock audioLock(m_AudioLoadMutex); // vs. the worker's LoadClip
				if (std::shared_ptr<AudioClip> clip = m_AudioEngine->LoadClip(absolutePath))
				{
					m_AudioClips[metadata.Handle] = std::move(clip);
					Utils::StampWriteTime(metadata, absolutePath);
					metadata.State = AssetState::Ready;
					return true;
				}
				break;
			}
			default:
				break;
		}

		DE_CORE_ERROR("AssetManager: failed to load {} '{}'.", AssetTypeToString(metadata.Type), absolutePath.string());
		metadata.State = AssetState::Failed;
		return false;
	}

	void AssetManager::UnloadInternal(const AssetMetadata& metadata)
	{
		switch (metadata.Type)
		{
			case AssetType::Texture2D:
			{
				auto it = m_Textures.find(metadata.Handle);
				if (it != m_Textures.end())
				{
					it->second->Destroy();
					delete it->second;
					m_Textures.erase(it);
				}
				break;
			}
			case AssetType::Shader:
			{
				auto it = m_Shaders.find(metadata.Handle);
				if (it != m_Shaders.end())
				{
					it->second->Destroy();
					delete it->second;
					m_Shaders.erase(it);
				}
				break;
			}
			case AssetType::Model:
			{
				auto it = m_Models.find(metadata.Handle);
				if (it != m_Models.end())
				{
					it->second->Destroy();
					delete it->second;
					m_Models.erase(it);
				}
				break;
			}
			case AssetType::Font:
			{
				auto it = m_Fonts.find(metadata.Handle);
				if (it != m_Fonts.end())
				{
					it->second->Destroy();
					delete it->second;
					m_Fonts.erase(it);
				}
				break;
			}
			case AssetType::AudioClip:
			{
				m_AudioClips.erase(metadata.Handle);
				break;
			}
			default:
				break;
		}

		m_Registry.at(metadata.Handle).State = AssetState::Unloaded;
	}

	std::string AssetManager::NormalizePathKey(const std::filesystem::path& path)
	{
		return path.lexically_normal().generic_string();
	}

}
