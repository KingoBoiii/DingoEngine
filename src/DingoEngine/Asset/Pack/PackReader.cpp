#include "depch.h"
#include "DingoEngine/Asset/Pack/PackReader.h"

#include "DingoEngine/Asset/Pack/Crc32.h"
#include "DingoEngine/Asset/Pack/Deflate.h"

#include <cstring>

#ifndef DE_PLATFORM_WINDOWS
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace Dingo::Internal
{

	std::shared_ptr<PackReader> PackReader::Open(const std::filesystem::path& path, const PackageKey& key, std::string& error)
	{
		std::shared_ptr<PackReader> reader(new PackReader());
		const std::u8string name = path.filename().u8string();
		reader->m_Name.assign(name.begin(), name.end());

#ifdef DE_PLATFORM_WINDOWS
		HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file == INVALID_HANDLE_VALUE)
		{
			error = "can't open the file";
			return nullptr;
		}
		reader->m_File = file;
		LARGE_INTEGER size{};
		if (!GetFileSizeEx(file, &size))
		{
			error = "can't read the file's size";
			return nullptr;
		}
		reader->m_FileSize = static_cast<uint64_t>(size.QuadPart);
#else
		reader->m_File = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
		if (reader->m_File < 0)
		{
			error = "can't open the file";
			return nullptr;
		}
		struct stat info{};
		if (::fstat(reader->m_File, &info) != 0)
		{
			error = "can't read the file's size";
			return nullptr;
		}
		reader->m_FileSize = static_cast<uint64_t>(info.st_size);
#endif

		Pack::Header header{};
		if (reader->m_FileSize < sizeof(header) || !reader->ReadAt(0, &header, sizeof(header))
			|| std::memcmp(header.Magic, Pack::k_Magic, sizeof(header.Magic)) != 0)
		{
			error = "not a .dpak package";
			return nullptr;
		}
		if (header.Version != Pack::k_Version)
		{
			error = std::format("package version {} (this engine reads version {})", header.Version, Pack::k_Version);
			return nullptr;
		}

		reader->m_Encrypted = (header.Flags & Pack::HeaderFlag_Encrypted) != 0;
		std::memcpy(reader->m_Salt, header.Salt, sizeof(header.Salt));
		if (reader->m_Encrypted)
		{
			if (key.IsNull())
			{
				error = "the package is encrypted and no key was given";
				return nullptr;
			}
			reader->m_Key = key.Bytes;

			uint8_t keystream[64];
			ChaCha20::Block(reader->m_Key, 0, Pack::MakeNonce(reader->m_Salt, Pack::k_KeyCheckNonceIndex), keystream);
			if (std::memcmp(keystream, header.KeyCheck, sizeof(header.KeyCheck)) != 0)
			{
				error = "wrong key";
				return nullptr;
			}
		}

		if (header.TocOffset < sizeof(header) || header.TocOffset > reader->m_FileSize || header.TocPackedSize != reader->m_FileSize - header.TocOffset
			|| header.TocPackedSize > Pack::k_MaxEntrySize || header.TocRawSize > Pack::k_MaxEntrySize || header.EntryCount > Pack::k_MaxEntries)
		{
			error = "damaged header";
			return nullptr;
		}

		std::vector<uint8_t> toc;
		if (header.EntryCount > 0)
		{
			std::vector<uint8_t> packedToc(static_cast<size_t>(header.TocPackedSize));
			if (!reader->ReadAt(header.TocOffset, packedToc.data(), packedToc.size()))
			{
				error = "can't read the table of contents";
				return nullptr;
			}
			if (reader->m_Encrypted)
				ChaCha20::Apply(reader->m_Key, Pack::MakeNonce(reader->m_Salt, Pack::k_TocNonceIndex), 0, packedToc.data(), packedToc.size());
			std::optional<std::vector<uint8_t>> inflated = Inflate(packedToc.data(), packedToc.size(), static_cast<size_t>(header.TocRawSize));
			if (!inflated || Crc32(inflated->data(), inflated->size()) != header.TocCrc)
			{
				error = "damaged table of contents";
				return nullptr;
			}
			toc = std::move(*inflated);
		}

		size_t cursor = 0;
		reader->m_Entries.reserve(header.EntryCount);
		for (uint32_t i = 0; i < header.EntryCount; ++i)
		{
			EntryInfo entry;
			if (toc.size() - cursor < sizeof(Pack::TocEntry))
			{
				error = "damaged table of contents";
				return nullptr;
			}
			std::memcpy(&entry.Toc, toc.data() + cursor, sizeof(Pack::TocEntry));
			cursor += sizeof(Pack::TocEntry);

			if (entry.Toc.NameLength == 0 || toc.size() - cursor < entry.Toc.NameLength || entry.Toc.Offset < sizeof(header)
				|| entry.Toc.Offset > header.TocOffset || entry.Toc.PackedSize > header.TocOffset - entry.Toc.Offset || entry.Toc.RawSize > Pack::k_MaxEntrySize)
			{
				error = "damaged table of contents";
				return nullptr;
			}
			entry.Name.assign(reinterpret_cast<const char*>(toc.data() + cursor), entry.Toc.NameLength);
			cursor += entry.Toc.NameLength;

			if (!reader->m_Lookup.emplace(Pack::FoldCase(entry.Name), i).second)
			{
				error = "two entries named '" + entry.Name + "'";
				return nullptr;
			}
			reader->m_Entries.push_back(std::move(entry));
		}
		return reader;
	}

	PackReader::~PackReader()
	{
#ifdef DE_PLATFORM_WINDOWS
		if (m_File)
			CloseHandle(static_cast<HANDLE>(m_File));
#else
		if (m_File >= 0)
			::close(m_File);
#endif
	}

	bool PackReader::ReadAt(uint64_t offset, void* destination, size_t size) const
	{
		uint8_t* out = static_cast<uint8_t*>(destination);
		while (size > 0)
		{
#ifdef DE_PLATFORM_WINDOWS
			OVERLAPPED overlapped{};
			overlapped.Offset = static_cast<DWORD>(offset);
			overlapped.OffsetHigh = static_cast<DWORD>(offset >> 32);
			const DWORD chunk = static_cast<DWORD>((std::min)(size, size_t(1) << 30));
			DWORD read = 0;
			if (!ReadFile(static_cast<HANDLE>(m_File), out, chunk, &read, &overlapped) || read == 0)
				return false;
#else
			const ssize_t read = ::pread(m_File, out, (std::min)(size, size_t(1) << 30), static_cast<off_t>(offset));
			if (read <= 0)
				return false;
#endif
			out += read;
			offset += static_cast<uint64_t>(read);
			size -= static_cast<size_t>(read);
		}
		return true;
	}

	std::optional<uint32_t> PackReader::Find(std::string_view key) const
	{
		auto it = m_Lookup.find(Pack::FoldCase(key));
		if (it == m_Lookup.end())
			return std::nullopt;

#ifndef DE_DISTRIBUTION
		if (m_Entries[it->second].Name != key)
		{
			std::scoped_lock lock(m_WarnMutex);
			if (m_CaseWarned.insert(it->second).second)
				DE_CORE_WARN("Package '{}': '{}' is stored as '{}'; on Linux the loose file needs the same case.", m_Name, key, m_Entries[it->second].Name);
		}
#endif
		return it->second;
	}

	std::optional<std::vector<uint8_t>> PackReader::Read(uint32_t index) const
	{
		if (index >= m_Entries.size())
			return std::nullopt;

		const EntryInfo& entry = m_Entries[index];
		std::vector<uint8_t> bytes(static_cast<size_t>(entry.Toc.PackedSize));
		if (!ReadAt(entry.Toc.Offset, bytes.data(), bytes.size()))
		{
			DE_CORE_ERROR("Package '{}': can't read '{}'.", m_Name, entry.Name);
			return std::nullopt;
		}

		if (entry.Toc.Flags & Pack::EntryFlag_Encrypted)
		{
			if (!m_Encrypted)
			{
				DE_CORE_ERROR("Package '{}': '{}' is damaged (encrypted in an unencrypted package).", m_Name, entry.Name);
				return std::nullopt;
			}
			ChaCha20::Apply(m_Key, Pack::MakeNonce(m_Salt, index), 0, bytes.data(), bytes.size());
		}

		if (entry.Toc.Flags & Pack::EntryFlag_Deflated)
		{
			std::optional<std::vector<uint8_t>> inflated = Inflate(bytes.data(), bytes.size(), static_cast<size_t>(entry.Toc.RawSize));
			if (!inflated)
			{
				DE_CORE_ERROR("Package '{}': '{}' is damaged (it doesn't inflate).", m_Name, entry.Name);
				return std::nullopt;
			}
			bytes = std::move(*inflated);
		}

		if (bytes.size() != entry.Toc.RawSize || Crc32(bytes.data(), bytes.size()) != entry.Toc.Crc)
		{
			DE_CORE_ERROR("Package '{}': '{}' is damaged (CRC mismatch).", m_Name, entry.Name);
			return std::nullopt;
		}
		return bytes;
	}

}
