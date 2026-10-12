#include "depch.h"
#include "DingoEngine/Core/VirtualFileSystem.h"
#include "DingoEngine/Core/PathText.h"

#include "DingoEngine/Core/VfsArchive.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <fstream>
#include <mutex>
#include <shared_mutex>

namespace Dingo
{

	namespace
	{

		struct Mount
		{
			VfsMountId Id = k_InvalidVfsMount;
			std::filesystem::path Directory;
			std::string DirectoryKey;
			std::shared_ptr<Internal::VfsArchive> Archive;
		};

		std::shared_mutex s_Mutex;
		std::vector<Mount> s_Mounts;
		std::filesystem::path s_Root;
		std::string s_RootKey;
		std::string s_RootName;
		VfsMountId s_NextMountId = 1;

		bool SameText(std::string_view a, std::string_view b)
		{
#ifdef DE_PLATFORM_WINDOWS
			return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](unsigned char l, unsigned char r) { return std::tolower(l) == std::tolower(r); });
#else
			return a == b;
#endif
		}

		// Keys are UTF-8, the encoding package entry names are stored in, on every platform.
		std::string NormalizedGeneric(const std::filesystem::path& path)
		{
			const std::u8string utf8 = path.lexically_normal().generic_u8string();
			std::string generic(utf8.begin(), utf8.end());
			while (generic.size() > 1 && generic.back() == '/')
				generic.pop_back();
			return generic;
		}

		std::filesystem::path FromUtf8(const std::string& key)
		{
			return std::filesystem::path(std::u8string(key.begin(), key.end()));
		}

		std::optional<std::string> RelativeTo(std::string_view path, std::string_view prefix)
		{
			if (prefix.empty() || path.size() <= prefix.size() + 1 || path[prefix.size()] != '/')
				return std::nullopt;
			if (!SameText(path.substr(0, prefix.size()), prefix))
				return std::nullopt;
			return std::string(path.substr(prefix.size() + 1));
		}

		// Caller holds s_Mutex.
		std::optional<std::string> FoldAbsolute(const std::string& generic)
		{
			if (auto key = RelativeTo(generic, s_RootKey))
				return key;
			for (auto it = s_Mounts.rbegin(); it != s_Mounts.rend(); ++it)
			{
				if (it->Archive)
					continue;
				if (auto key = RelativeTo(generic, it->DirectoryKey))
					return key;
			}
			return std::nullopt;
		}

		bool IsRegularFile(const std::filesystem::path& path)
		{
			std::error_code ec;
			return std::filesystem::is_regular_file(path, ec) && !ec;
		}

		struct Found
		{
			VfsResolvedPath Path;
			const Internal::VfsArchive* Archive = nullptr;
			uint32_t Entry = 0;
		};

		// Caller holds s_Mutex.
		std::optional<Found> FindInMounts(const std::string& key)
		{
			for (auto it = s_Mounts.rbegin(); it != s_Mounts.rend(); ++it)
			{
				if (it->Archive)
				{
					if (key.starts_with("../"))
						continue;
					if (std::optional<uint32_t> entry = it->Archive->Find(key))
						return Found{ { it->Archive->GetEntryName(*entry), {}, it->Id }, it->Archive.get(), *entry };
					continue;
				}

				const std::filesystem::path file = (it->Directory / FromUtf8(key)).lexically_normal();
				if (IsRegularFile(file))
					return Found{ { key, file, it->Id } };
			}
			return std::nullopt;
		}

		// Caller holds s_Mutex.
		std::optional<Found> ResolveLocked(const std::filesystem::path& path)
		{
			if (path.empty())
				return std::nullopt;

			const std::string generic = NormalizedGeneric(path);
			if (path.is_absolute())
			{
				if (std::optional<std::string> key = FoldAbsolute(generic))
					return FindInMounts(*key);
				if (IsRegularFile(path))
					return Found{ { {}, path, k_InvalidVfsMount } };
				return std::nullopt;
			}

			if (generic != "." && generic != "..")
			{
				if (std::optional<Found> found = FindInMounts(generic))
					return found;

				if (!s_RootName.empty())
				{
					if (std::optional<std::string> stripped = RelativeTo(generic, s_RootName))
					{
						if (std::optional<Found> found = FindInMounts(*stripped))
						{
#ifdef DE_DEBUG
							static std::atomic<bool> s_Warned = false;
							if (!s_Warned.exchange(true))
								DE_CORE_WARN("VirtualFileSystem: '{}' names the asset root's folder; write it root-relative ('{}'). Warned once.", generic, *stripped);
#endif
							return found;
						}
					}
				}
			}

			if (IsRegularFile(path))
				return Found{ { {}, path, k_InvalidVfsMount } };
			return std::nullopt;
		}

		std::optional<std::vector<uint8_t>> ReadDiskFile(const std::filesystem::path& path)
		{
			std::error_code ec;
			const uintmax_t size = std::filesystem::file_size(path, ec);
			if (ec)
				return std::nullopt;

			std::ifstream stream(path, std::ios::binary);
			if (!stream)
				return std::nullopt;

			std::vector<uint8_t> bytes(static_cast<size_t>(size));
			stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
			if (static_cast<uintmax_t>(stream.gcount()) != size)
				return std::nullopt;
			return bytes;
		}

		VfsMountId AddMount(Mount mount)
		{
			std::unique_lock lock(s_Mutex);
			mount.Id = s_NextMountId++;
			s_Mounts.push_back(std::move(mount));
			return s_Mounts.back().Id;
		}

	}

	namespace Internal
	{

		VfsMountId MountArchive(std::shared_ptr<VfsArchive> archive)
		{
			if (!archive)
				return k_InvalidVfsMount;

			Mount mount;
			mount.Archive = std::move(archive);
			return AddMount(std::move(mount));
		}

	}

	void VirtualFileSystem::SetRoot(const std::filesystem::path& root)
	{
		std::unique_lock lock(s_Mutex);
		s_Root = root.empty() ? std::filesystem::path() : root.lexically_normal();
		s_RootKey = root.empty() ? std::string() : NormalizedGeneric(root);
		s_RootName = s_Root.has_filename() ? NormalizedGeneric(s_Root.filename()) : std::string();
	}

	const std::filesystem::path& VirtualFileSystem::GetRoot()
	{
		return s_Root;
	}

	VfsMountId VirtualFileSystem::MountDirectory(const std::filesystem::path& directory)
	{
		if (directory.empty())
			return k_InvalidVfsMount;

		Mount mount;
		mount.Directory = std::filesystem::absolute(directory).lexically_normal();
		mount.DirectoryKey = NormalizedGeneric(mount.Directory);
		return AddMount(std::move(mount));
	}

	void VirtualFileSystem::Unmount(VfsMountId mount)
	{
		std::unique_lock lock(s_Mutex);
		std::erase_if(s_Mounts, [mount](const Mount& m) { return m.Id == mount; });
	}

	bool VirtualFileSystem::IsMounted(VfsMountId mount)
	{
		std::shared_lock lock(s_Mutex);
		return std::any_of(s_Mounts.begin(), s_Mounts.end(), [mount](const Mount& m) { return m.Id == mount; });
	}

	uint32_t VirtualFileSystem::GetMountCount()
	{
		std::shared_lock lock(s_Mutex);
		return static_cast<uint32_t>(s_Mounts.size());
	}

	std::string VirtualFileSystem::GetMountName(VfsMountId mount)
	{
		std::shared_lock lock(s_Mutex);
		for (const Mount& m : s_Mounts)
		{
			if (m.Id == mount)
				return m.Archive ? m.Archive->GetName() : Internal::PathToGenericUtf8(m.Directory);
		}
		return {};
	}

	std::optional<VfsResolvedPath> VirtualFileSystem::Resolve(const std::filesystem::path& path)
	{
		std::shared_lock lock(s_Mutex);
		std::optional<Found> found = ResolveLocked(path);
		if (!found)
			return std::nullopt;
		return std::move(found->Path);
	}

	bool VirtualFileSystem::Exists(const std::filesystem::path& path)
	{
		std::shared_lock lock(s_Mutex);
		return ResolveLocked(path).has_value();
	}

	std::optional<std::vector<uint8_t>> VirtualFileSystem::ReadBytes(const std::filesystem::path& path)
	{
		std::shared_lock lock(s_Mutex);
		std::optional<Found> found = ResolveLocked(path);
		if (!found)
			return std::nullopt;
		if (found->Archive)
			return found->Archive->Read(found->Entry);
		return ReadDiskFile(found->Path.DiskPath);
	}

	std::optional<std::filesystem::file_time_type> VirtualFileSystem::GetWriteTime(const std::filesystem::path& path)
	{
		std::optional<VfsResolvedPath> resolved = Resolve(path);
		if (!resolved || resolved->DiskPath.empty())
			return std::nullopt;

		std::error_code ec;
		const std::filesystem::file_time_type time = std::filesystem::last_write_time(resolved->DiskPath, ec);
		if (ec)
			return std::nullopt;
		return time;
	}

	bool VirtualFileSystem::IsLooseFile(const std::filesystem::path& path)
	{
		std::optional<VfsResolvedPath> resolved = Resolve(path);
		return resolved && !resolved->DiskPath.empty();
	}

	std::filesystem::path VirtualFileSystem::GetVirtualPath(const std::filesystem::path& path)
	{
		std::shared_lock lock(s_Mutex);
		std::optional<Found> found = ResolveLocked(path);
		if (!found || found->Path.Mount == k_InvalidVfsMount)
			return path;
		if (!found->Path.DiskPath.empty())
			return found->Path.DiskPath;
		return (s_Root / FromUtf8(found->Path.LogicalKey)).lexically_normal();
	}

	std::string VirtualFileSystem::GetLogicalKey(const std::filesystem::path& path)
	{
		std::shared_lock lock(s_Mutex);
		if (std::optional<Found> found = ResolveLocked(path); found && !found->Path.LogicalKey.empty())
			return found->Path.LogicalKey;

		const std::string generic = NormalizedGeneric(path);
		if (path.is_absolute())
		{
			if (std::optional<std::string> key = FoldAbsolute(generic))
				return *key;
		}
		return generic;
	}

}
