#pragma once
#include "DingoEngine/Asset/AssetTypes.h"

#include <filesystem>

namespace Dingo
{

	struct AssetMetadata
	{
		AssetHandle Handle = k_InvalidAsset;
		AssetType Type = AssetType::None;
		// Relative to the AssetManager's root directory (kept relative so registrations
		// stay valid if the root moves between runs/machines).
		std::filesystem::path FilePath;
		// FilePath resolved against the root once at registration. The root is fixed for the
		// manager's lifetime, so this cannot go stale, and it keeps the hot-reload poll from
		// rebuilding a path per watched asset per tick.
		std::filesystem::path AbsolutePath;
		AssetState State = AssetState::Unloaded;
		// Source-file timestamp at load; the hot-reload poll compares against it.
		std::filesystem::file_time_type LastWriteTime{};
		// A changed timestamp the poll has seen once but not yet acted on. It must repeat on
		// the next poll before the write counts as finished — see PollHotReload.
		std::filesystem::file_time_type PendingWriteTime{};
	};

}
