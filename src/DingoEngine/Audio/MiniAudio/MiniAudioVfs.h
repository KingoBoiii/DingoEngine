#pragma once

#include <filesystem>
#include <string>

namespace Dingo::Internal
{

	// An ma_vfs over the VirtualFileSystem, handed to the engine's resource manager so clips load
	// from packages too. Opaque here so miniaudio stays out of every header.
	struct MiniAudioVfs;

	MiniAudioVfs* CreateMiniAudioVfs();
	void DestroyMiniAudioVfs(MiniAudioVfs* vfs);

	// The path as the VFS callbacks expect it: UTF-8, forward slashes.
	std::string ToMiniAudioPath(const std::filesystem::path& path);

}
