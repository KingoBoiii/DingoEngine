#pragma once
#include <DingoEngine/Core/SaveData.h>

#include <filesystem>

namespace Dingo::Test
{

	// Large enough that a write takes a while, so a kill lands inside one.
	inline SaveData MakeWriterSave(int64_t generation)
	{
		SaveData data;
		data["generation"] = generation;
		SaveData& items = data["items"];
		items = SaveData::MakeArray();
		for (int i = 0; i < 2000; i++)
			items.Push(generation * 100000 + i);
		data["text"] = std::string(256 * 1024, static_cast<char>('a' + generation % 26));
		return data;
	}

	// The child process of the kill-mid-write test: writes ever newer generations until killed.
	[[noreturn]] inline void RunSaveWriter(const std::filesystem::path& path)
	{
		for (int64_t generation = 1;; generation++)
			MakeWriterSave(generation).WriteFile(path);
	}

}
