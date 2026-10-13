#include "depch.h"
#include "DingoEngine/Audio/MiniAudio/MiniAudioVfs.h"

#include "DingoEngine/Core/VirtualFileSystem.h"

#include <cstring>
#include <vector>

#include "miniaudio.h"

namespace Dingo::Internal
{

	struct MiniAudioVfs
	{
		// First, so a MiniAudioVfs* is the ma_vfs* miniaudio casts to its callbacks.
		ma_vfs_callbacks Callbacks;
	};

	namespace
	{

		struct OpenFile
		{
			std::vector<uint8_t> Bytes;
			size_t Position = 0;
		};

		ma_result OpenPath(const std::filesystem::path& path, ma_uint32 openMode, ma_vfs_file* file)
		{
			if (!file)
				return MA_INVALID_ARGS;
			*file = nullptr;
			if ((openMode & MA_OPEN_MODE_WRITE) != 0)
				return MA_ACCESS_DENIED;

			std::optional<std::vector<uint8_t>> bytes = VirtualFileSystem::ReadBytes(path);
			if (!bytes)
				return MA_DOES_NOT_EXIST;

			*file = new OpenFile{ std::move(*bytes), 0 };
			return MA_SUCCESS;
		}

		ma_result OnOpen(ma_vfs*, const char* filePath, ma_uint32 openMode, ma_vfs_file* file)
		{
			if (!filePath)
				return MA_INVALID_ARGS;
			const std::string utf8(filePath);
			return OpenPath(std::filesystem::path(std::u8string(utf8.begin(), utf8.end())), openMode, file);
		}

		ma_result OnOpenW(ma_vfs*, const wchar_t* filePath, ma_uint32 openMode, ma_vfs_file* file)
		{
			if (!filePath)
				return MA_INVALID_ARGS;
			return OpenPath(std::filesystem::path(std::wstring(filePath)), openMode, file);
		}

		ma_result OnClose(ma_vfs*, ma_vfs_file file)
		{
			delete static_cast<OpenFile*>(file);
			return MA_SUCCESS;
		}

		ma_result OnRead(ma_vfs*, ma_vfs_file file, void* destination, size_t sizeInBytes, size_t* bytesRead)
		{
			OpenFile* open = static_cast<OpenFile*>(file);
			const size_t count = (std::min)(sizeInBytes, open->Bytes.size() - open->Position);
			if (count > 0)
				std::memcpy(destination, open->Bytes.data() + open->Position, count);
			open->Position += count;
			if (bytesRead)
				*bytesRead = count;
			return count == 0 && sizeInBytes > 0 ? MA_AT_END : MA_SUCCESS;
		}

		ma_result OnWrite(ma_vfs*, ma_vfs_file, const void*, size_t, size_t* bytesWritten)
		{
			if (bytesWritten)
				*bytesWritten = 0;
			return MA_ACCESS_DENIED;
		}

		ma_result OnSeek(ma_vfs*, ma_vfs_file file, ma_int64 offset, ma_seek_origin origin)
		{
			OpenFile* open = static_cast<OpenFile*>(file);
			ma_int64 base = 0;
			if (origin == ma_seek_origin_current)
				base = static_cast<ma_int64>(open->Position);
			else if (origin == ma_seek_origin_end)
				base = static_cast<ma_int64>(open->Bytes.size());

			const ma_int64 target = base + offset;
			if (target < 0 || target > static_cast<ma_int64>(open->Bytes.size()))
				return MA_BAD_SEEK;
			open->Position = static_cast<size_t>(target);
			return MA_SUCCESS;
		}

		ma_result OnTell(ma_vfs*, ma_vfs_file file, ma_int64* cursor)
		{
			*cursor = static_cast<ma_int64>(static_cast<OpenFile*>(file)->Position);
			return MA_SUCCESS;
		}

		ma_result OnInfo(ma_vfs*, ma_vfs_file file, ma_file_info* info)
		{
			info->sizeInBytes = static_cast<ma_uint64>(static_cast<OpenFile*>(file)->Bytes.size());
			return MA_SUCCESS;
		}

	}

	MiniAudioVfs* CreateMiniAudioVfs()
	{
		MiniAudioVfs* vfs = new MiniAudioVfs();
		vfs->Callbacks = { OnOpen, OnOpenW, OnClose, OnRead, OnWrite, OnSeek, OnTell, OnInfo };
		return vfs;
	}

	void DestroyMiniAudioVfs(MiniAudioVfs* vfs)
	{
		delete vfs;
	}

	std::string ToMiniAudioPath(const std::filesystem::path& path)
	{
		const std::u8string utf8 = path.generic_u8string();
		return std::string(utf8.begin(), utf8.end());
	}

}
