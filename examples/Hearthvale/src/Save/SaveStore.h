#pragma once
#include "Save/SaveGame.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace Dingo
{

	enum class SlotState : uint8_t { Empty, Ok, Damaged };

	struct SlotRead
	{
		SlotState State = SlotState::Empty;
		bool FromBackup = false;    // the file was missing, cut short or failed the game's checks, and its .bak or .tmp was read
		std::string Error;
	};

	// Named saves in one directory, through SaveData's atomic files: the game's own is SaveSlots::GetDirectory()
	// (so the engine's --save-dir moves it), the checks' a scratch directory.
	class SaveStore
	{
	public:
		explicit SaveStore(std::filesystem::path directory) : m_Directory(std::move(directory)) {}

		const std::filesystem::path& GetDirectory() const { return m_Directory; }
		std::filesystem::path PathOf(std::string_view name) const;

		bool Write(std::string_view name, const SaveData& save) const;
		// The file, its .bak or a leftover .tmp is on disk.
		bool Exists(std::string_view name) const;
		bool Delete(std::string_view name) const;

		// Reads, migrates and checks the save. A file that parses but fails the game's checks is treated like one that
		// does not parse: its .bak is tried, with a warning. `out` changes only when the result is Ok.
		SlotRead Read(std::string_view name, LoadedGame& out) const;

	private:
		std::filesystem::path FileOf(std::string_view name, std::string_view suffix) const;

	private:
		std::filesystem::path m_Directory;
	};

}
