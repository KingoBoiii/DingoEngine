#include "depch.h"
#include "DingoEngine/Asset/Pack/PackWriter.h"

#include "DingoEngine/Asset/Pack/Crc32.h"
#include "DingoEngine/Asset/Pack/Deflate.h"

#include <cstring>
#include <random>

namespace Dingo::Internal
{

	namespace
	{

		bool IsAlreadyCompressed(const std::string& name)
		{
			const std::string folded = Pack::FoldCase(name);
			for (std::string_view extension : { ".png", ".jpg", ".jpeg", ".ogg", ".mp3" })
			{
				if (folded.ends_with(extension))
					return true;
			}
			return false;
		}

	}

	PackWriter::PackWriter(const PackageKey& key)
		: m_Key(key.Bytes), m_Encrypted(!key.IsNull())
	{
		std::random_device random;
		for (size_t i = 0; i < sizeof(m_Salt); i += 4)
		{
			const uint32_t value = random();
			std::memcpy(m_Salt + i, &value, 4);
		}
		m_Blob.resize(sizeof(Pack::Header));
	}

	bool PackWriter::Add(const std::string& name, const std::vector<uint8_t>& data, std::string& error)
	{
		if (name.empty() || name.size() > 0xFFFF)
		{
			error = "'" + name + "': the name is empty or too long";
			return false;
		}
		if (data.size() > Pack::k_MaxEntrySize)
		{
			error = "'" + name + "' is larger than 2 GiB";
			return false;
		}
		if (m_Entries.size() >= Pack::k_MaxEntries)
		{
			error = "too many files";
			return false;
		}

		Entry entry;
		entry.Name = name;
		entry.Toc.PathHash = Pack::HashPath(name);
		entry.Toc.RawSize = data.size();
		entry.Toc.Crc = Crc32(data.data(), data.size());
		entry.Toc.NameLength = static_cast<uint16_t>(name.size());

		std::vector<uint8_t> stored;
		if (!IsAlreadyCompressed(name) && !data.empty())
		{
			std::optional<std::vector<uint8_t>> deflated = Deflate(data.data(), data.size());
			if (deflated && deflated->size() * 100 <= data.size() * 95)
			{
				stored = std::move(*deflated);
				entry.Toc.Flags |= Pack::EntryFlag_Deflated;
			}
		}
		if (!(entry.Toc.Flags & Pack::EntryFlag_Deflated))
			stored = data;

		if (m_Encrypted)
		{
			ChaCha20::Apply(m_Key, Pack::MakeNonce(m_Salt, static_cast<uint32_t>(m_Entries.size())), 0, stored.data(), stored.size());
			entry.Toc.Flags |= Pack::EntryFlag_Encrypted;
		}

		entry.Toc.Offset = m_Blob.size();
		entry.Toc.PackedSize = stored.size();
		m_Blob.append(reinterpret_cast<const char*>(stored.data()), stored.size());
		m_RawBytes += data.size();
		m_Entries.push_back(std::move(entry));
		return true;
	}

	std::string PackWriter::Finish()
	{
		std::vector<uint8_t> toc;
		for (const Entry& entry : m_Entries)
		{
			const size_t at = toc.size();
			toc.resize(at + sizeof(Pack::TocEntry) + entry.Name.size());
			std::memcpy(toc.data() + at, &entry.Toc, sizeof(Pack::TocEntry));
			std::memcpy(toc.data() + at + sizeof(Pack::TocEntry), entry.Name.data(), entry.Name.size());
		}

		Pack::Header header{};
		std::memcpy(header.Magic, Pack::k_Magic, sizeof(header.Magic));
		header.Version = Pack::k_Version;
		header.Flags = m_Encrypted ? Pack::HeaderFlag_Encrypted : 0;
		std::memcpy(header.Salt, m_Salt, sizeof(m_Salt));
		header.TocRawSize = toc.size();
		header.TocCrc = Crc32(toc.data(), toc.size());
		header.EntryCount = static_cast<uint32_t>(m_Entries.size());

		std::vector<uint8_t> packedToc = Deflate(toc.data(), toc.size()).value_or(std::vector<uint8_t>());

		if (m_Encrypted)
		{
			ChaCha20::Apply(m_Key, Pack::MakeNonce(m_Salt, Pack::k_TocNonceIndex), 0, packedToc.data(), packedToc.size());

			uint8_t keystream[64];
			ChaCha20::Block(m_Key, 0, Pack::MakeNonce(m_Salt, Pack::k_KeyCheckNonceIndex), keystream);
			std::memcpy(header.KeyCheck, keystream, sizeof(header.KeyCheck));
		}

		header.TocOffset = m_Blob.size();
		header.TocPackedSize = packedToc.size();
		m_Blob.append(reinterpret_cast<const char*>(packedToc.data()), packedToc.size());
		std::memcpy(m_Blob.data(), &header, sizeof(header));
		return std::move(m_Blob);
	}

}
