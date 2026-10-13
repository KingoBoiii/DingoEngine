#pragma once

#include "DingoEngine/Core/VirtualFileSystem.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Dingo::Internal
{

	// A mounted package as the VFS sees it. Both calls may run on several threads at once.
	class VfsArchive
	{
	public:
		virtual ~VfsArchive() = default;

		// The entry's index when the archive holds `key`, which it matches regardless of case.
		virtual std::optional<uint32_t> Find(std::string_view key) const = 0;
		virtual const std::string& GetEntryName(uint32_t entry) const = 0;
		virtual std::optional<std::vector<uint8_t>> Read(uint32_t entry) const = 0;
		virtual const std::string& GetName() const = 0;
	};

	VfsMountId MountArchive(std::shared_ptr<VfsArchive> archive);

}
