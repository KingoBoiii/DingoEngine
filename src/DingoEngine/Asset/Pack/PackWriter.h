#pragma once

#include "DingoEngine/Asset/AssetPackage.h"
#include "DingoEngine/Asset/Pack/PackFormat.h"

#include <string>
#include <vector>

namespace Dingo::Internal
{

	// Builds a whole .dpak in memory.
	class PackWriter
	{
	public:
		PackWriter();

		// `name` is the entry's logical key. False, with `error` set, for an entry the format can't hold.
		bool Add(const std::string& name, const std::vector<uint8_t>& data, std::string& error);
		std::string Finish();

		uint32_t GetEntryCount() const { return static_cast<uint32_t>(m_Entries.size()); }
		uint64_t GetRawBytes() const { return m_RawBytes; }

	private:
		struct Entry
		{
			Pack::TocEntry Toc{};
			std::string Name;
		};

		uint8_t m_Salt[16]{};
		std::string m_Blob;
		std::vector<Entry> m_Entries;
		uint64_t m_RawBytes = 0;
	};

}
