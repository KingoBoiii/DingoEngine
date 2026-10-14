#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Dingo
{

	/// Identifies one mount (a directory or a package); ids are never reused within a run.
	using VfsMountId = uint64_t;
	/// The "no mount" id: returned when a mount fails, and in VfsResolvedPath for a file outside
	/// every mount.
	inline constexpr VfsMountId k_InvalidVfsMount = 0;

	/// Where VirtualFileSystem::Resolve found a file.
	struct VfsResolvedPath
	{
		/// Root-relative with forward slashes, the name a package stores the file under. Empty for
		/// a disk file outside every mount (an absolute path elsewhere, or the working-directory
		/// fallback).
		std::string LogicalKey;
		/// Empty when the file comes from a package.
		std::filesystem::path DiskPath;
		/// k_InvalidVfsMount for a disk file outside every mount.
		VfsMountId Mount = k_InvalidVfsMount;
	};

	/// Every engine file read goes through here, so a file can come from a loose directory or a
	/// mounted package alike. Mounts are ordered: a later mount wins over an earlier one for the
	/// same logical path, so a patch package can replace single files.
	///
	/// A path is looked up by its logical key: a relative path as written, or an absolute path
	/// under the root (or under a mounted directory) relative to it. A relative path that misses is
	/// tried again without a leading folder named like the root ("assets/x.png" when the root is
	/// .../assets), then as a working-directory-relative disk file; an absolute path outside every
	/// mount is read from disk as it is.
	///
	/// Thread-safe: reads run on the asset loader thread too. Mounting and unmounting take an
	/// exclusive lock and belong at startup and shutdown.
	class VirtualFileSystem
	{
	public:
		/// The absolute directory logical keys are relative to, and that GetVirtualPath builds on.
		/// The AssetManager sets it to its root; empty means no root (relative paths read from the
		/// cwd).
		static void SetRoot(const std::filesystem::path& root);
		/// The root set by SetRoot, normalized; empty when none is set.
		static const std::filesystem::path& GetRoot();

		/// Mounts a loose directory on top of the earlier mounts and returns its id, or
		/// k_InvalidVfsMount for an empty path. Startup/shutdown only: it takes the exclusive lock.
		static VfsMountId MountDirectory(const std::filesystem::path& directory);
		/// Removes the mount; an unknown id does nothing. Startup/shutdown only.
		static void Unmount(VfsMountId mount);
		/// True while the id names a live mount.
		static bool IsMounted(VfsMountId mount);
		/// How many mounts are live, directories and packages together.
		static uint32_t GetMountCount();
		/// The directory's path or the package's file name, for logs and tooling.
		static std::string GetMountName(VfsMountId mount);

		/// Finds the file `path` names (see the class comment for the lookup rules); nullopt when
		/// no mount has it and it is not on disk.
		static std::optional<VfsResolvedPath> Resolve(const std::filesystem::path& path);
		/// True when Resolve would find the file.
		static bool Exists(const std::filesystem::path& path);
		/// The file's bytes from its package or disk, or nullopt when it is not found or can't be
		/// read.
		static std::optional<std::vector<uint8_t>> ReadBytes(const std::filesystem::path& path);
		/// nullopt for a packed or missing file.
		static std::optional<std::filesystem::file_time_type> GetWriteTime(const std::filesystem::path& path);
		/// True for a file that is read from disk, which is what hot reload can watch.
		static bool IsLooseFile(const std::filesystem::path& path);

		/// The path a factory should carry for `path`: `<root>/<key>` for a file found in a mount
		/// (the file itself for a directory mount other than the root's), else `path` unchanged.
		static std::filesystem::path GetVirtualPath(const std::filesystem::path& path);
		/// The logical key for `path` when it names something inside the root or a mounted
		/// directory, whether or not the file exists; otherwise its normalized generic spelling.
		static std::string GetLogicalKey(const std::filesystem::path& path);
	};

}
