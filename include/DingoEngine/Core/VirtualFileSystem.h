#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Dingo
{

	using VfsMountId = uint64_t;
	inline constexpr VfsMountId k_InvalidVfsMount = 0;

	struct VfsResolvedPath
	{
		// Root-relative with forward slashes, the name a package stores the file under. Empty for a
		// disk file outside every mount (an absolute path elsewhere, or the working-directory fallback).
		std::string LogicalKey;
		// Empty when the file comes from a package.
		std::filesystem::path DiskPath;
		// k_InvalidVfsMount for a disk file outside every mount.
		VfsMountId Mount = k_InvalidVfsMount;
	};

	// Every engine file read goes through here, so a file can come from a loose directory or a mounted
	// package alike. Mounts are ordered: a later mount wins over an earlier one for the same logical
	// path, so a patch package can replace single files.
	//
	// A path is looked up by its logical key: a relative path as written, or an absolute path under
	// the root (or under a mounted directory) relative to it. A relative path that misses is tried
	// again without a leading folder named like the root ("assets/x.png" when the root is .../assets),
	// then as a working-directory-relative disk file; an absolute path outside every mount is read
	// from disk as it is.
	//
	// Thread-safe: reads run on the asset loader thread too. Mounting and unmounting take an
	// exclusive lock and belong at startup and shutdown.
	class VirtualFileSystem
	{
	public:
		// The absolute directory logical keys are relative to, and that GetVirtualPath builds on. The
		// AssetManager sets it to its root; empty means no root (relative paths read from the cwd).
		static void SetRoot(const std::filesystem::path& root);
		static const std::filesystem::path& GetRoot();

		static VfsMountId MountDirectory(const std::filesystem::path& directory);
		static void Unmount(VfsMountId mount);
		static bool IsMounted(VfsMountId mount);
		static uint32_t GetMountCount();
		// The directory's path or the package's file name, for logs and tooling.
		static std::string GetMountName(VfsMountId mount);

		static std::optional<VfsResolvedPath> Resolve(const std::filesystem::path& path);
		static bool Exists(const std::filesystem::path& path);
		static std::optional<std::vector<uint8_t>> ReadBytes(const std::filesystem::path& path);
		// nullopt for a packed or missing file.
		static std::optional<std::filesystem::file_time_type> GetWriteTime(const std::filesystem::path& path);
		// True for a file that is read from disk, which is what hot reload can watch.
		static bool IsLooseFile(const std::filesystem::path& path);

		// The path a factory should carry for `path`: <root>/<key> for a file found in a mount (the
		// file itself for a directory mount other than the root's), else `path` unchanged.
		static std::filesystem::path GetVirtualPath(const std::filesystem::path& path);
		// The logical key for `path` when it names something inside the root or a mounted directory,
		// whether or not the file exists; otherwise its normalized generic spelling.
		static std::string GetLogicalKey(const std::filesystem::path& path);
	};

}
