#pragma once

#include "DingoEngine/Asset/AssetPackage.h"
#include "DingoEngine/Asset/Pack/PackFormat.h"
#include "DingoEngine/Core/VfsArchive.h"

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Dingo::Internal
{

	// An open .dpak. Reads are positional, so any number of threads may read at once.
	class PackReader final : public VfsArchive
	{
	public:
		struct EntryInfo
		{
			std::string Name;
			Pack::TocEntry Toc{};
		};

		// nullptr with `error` set when the file is missing, isn't a package, the key is wrong or the
		// TOC is damaged. A null key opens only an unencrypted package.
		static std::shared_ptr<PackReader> Open(const std::filesystem::path& path, const PackageKey& key, std::string& error);
		~PackReader() override;

		std::optional<uint32_t> Find(std::string_view key) const override;
		const std::string& GetEntryName(uint32_t entry) const override { return m_Entries[entry].Name; }
		std::optional<std::vector<uint8_t>> Read(uint32_t entry) const override;
		const std::string& GetName() const override { return m_Name; }

		uint32_t GetEntryCount() const { return static_cast<uint32_t>(m_Entries.size()); }
		const EntryInfo& GetEntry(uint32_t entry) const { return m_Entries[entry]; }
		bool IsEncrypted() const { return m_Encrypted; }
		uint64_t GetFileSize() const { return m_FileSize; }

	private:
		PackReader() = default;
		bool ReadAt(uint64_t offset, void* destination, size_t size) const;

		std::string m_Name;
#ifdef DE_PLATFORM_WINDOWS
		void* m_File = nullptr;
#else
		int m_File = -1;
#endif
		uint64_t m_FileSize = 0;
		bool m_Encrypted = false;
		ChaCha20::Key m_Key{};
		uint8_t m_Salt[16]{};
		std::vector<EntryInfo> m_Entries;
		std::unordered_map<std::string, uint32_t> m_Lookup;

		mutable std::mutex m_WarnMutex;
		mutable std::unordered_set<uint32_t> m_CaseWarned;
	};

}
