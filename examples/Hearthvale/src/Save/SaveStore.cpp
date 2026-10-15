#include "Save/SaveStore.h"

#include <DingoEngine.h>

#include <optional>
#include <system_error>

namespace Dingo
{

	std::filesystem::path SaveStore::FileOf(std::string_view name, std::string_view suffix) const
	{
		std::string file(name);
		file += ".json";
		file += suffix;
		return m_Directory / FileSystem::PathFromUtf8(file);
	}

	std::filesystem::path SaveStore::PathOf(std::string_view name) const
	{
		return FileOf(name, "");
	}

	bool SaveStore::Write(std::string_view name, const SaveData& save) const
	{
		if (!SaveSlots::IsValidName(name))
			return false;
		std::error_code ec;
		std::filesystem::create_directories(m_Directory, ec);
		return save.WriteFile(PathOf(name));
	}

	bool SaveStore::Exists(std::string_view name) const
	{
		if (!SaveSlots::IsValidName(name))
			return false;
		std::error_code ec;
		for (const std::string_view suffix : { "", ".bak", ".tmp" })
		{
			if (std::filesystem::exists(FileOf(name, suffix), ec))
				return true;
		}
		return false;
	}

	bool SaveStore::Delete(std::string_view name) const
	{
		if (!SaveSlots::IsValidName(name))
			return false;
		bool ok = true;
		for (const std::string_view suffix : { "", ".bak", ".tmp" })
		{
			std::error_code ec;
			std::filesystem::remove(FileOf(name, suffix), ec);
			ok = ok && !ec;
		}
		return ok;
	}

	SlotRead SaveStore::Read(std::string_view name, LoadedGame& out) const
	{
		SlotRead result;
		if (!Exists(name))
			return result;

		const std::filesystem::path path = PathOf(name);
		std::string error;
		std::filesystem::path source;
		std::optional<SaveData> data = SaveData::ReadFile(path, &error, &source);
		if (data && ReadSave(*data, out, error))
		{
			result.State = SlotState::Ok;
			result.FromBackup = source != path;
			if (result.FromBackup)
				DE_WARN("Hearthvale: save '{}' could not be read; loaded '{}' instead", FileSystem::PathToUtf8(path), FileSystem::PathToUtf8(source));
			return result;
		}

		const std::filesystem::path backup = FileOf(name, ".bak");
		std::error_code ec;
		if (data && source == path && std::filesystem::exists(backup, ec))
		{
			DE_WARN("Hearthvale: save '{}' is damaged ({}); trying its backup", FileSystem::PathToUtf8(path), error);
			std::string backupError;
			std::optional<SaveData> older = SaveData::ReadFile(backup, &backupError);
			if (older && ReadSave(*older, out, backupError))
			{
				result.State = SlotState::Ok;
				result.FromBackup = true;
				DE_WARN("Hearthvale: loaded the backup '{}'", FileSystem::PathToUtf8(backup));
				return result;
			}
			error += "; backup: " + backupError;
		}

		result.State = SlotState::Damaged;
		result.Error = error.empty() ? std::string("unreadable") : error;
		DE_WARN("Hearthvale: save '{}' is damaged: {}", FileSystem::PathToUtf8(path), result.Error);
		return result;
	}

}
