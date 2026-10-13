#include <doctest/doctest.h>

#include <DingoEngine/Core/Platform.h>
#include <DingoEngine/Core/SaveData.h>

#include "SaveWriter.h"

#include <chrono>
#include <cmath>
#include <fstream>
#include <limits>
#include <random>
#include <thread>

#if defined(DE_PLATFORM_WINDOWS)
#include <Windows.h>
#else
#include <csignal>
#include <cstdlib>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

using namespace Dingo;

namespace
{
	enum class Difficulty { Easy = 1, Hard = 3 };

	struct TempDirectory
	{
		std::filesystem::path Path;

		explicit TempDirectory(const char* name)
		{
			std::random_device random;
			Path = std::filesystem::temp_directory_path() / (std::string("DingoUnitTests-") + name + "-" + std::to_string(random()));
			std::filesystem::create_directories(Path);
		}

		~TempDirectory()
		{
			std::error_code ignored;
			std::filesystem::remove_all(Path, ignored);
		}
	};

	void WriteText(const std::filesystem::path& path, std::string_view text)
	{
		std::ofstream stream(path, std::ios::binary | std::ios::trunc);
		stream << text;
	}

	SaveData MakeSample()
	{
		SaveData data;
		data["name"] = "Wanderer";
		data["level"] = 12;
		data["health"] = 0.75f;
		data["alive"] = true;
		data["nothing"] = SaveData();
		data["position"] = glm::vec3(1.5f, -2.0f, 1e-30f);
		data["cell"] = glm::ivec2(-3, 7);
		data["facing"] = glm::quat(0.5f, 0.5f, 0.5f, 0.5f);
		data["difficulty"] = Difficulty::Hard;
		data["big"] = std::numeric_limits<int64_t>::min();
		data["inventory"].Push("sword");
		data["inventory"].Push(3);
		data["inventory"].Push(SaveData::MakeObject())["charges"] = 2;
		data["flags"] = SaveData::MakeArray();
		data["accent"] = "\xC3\xA9t\xC3\xA9 \xE2\x82\xAC";
		return data;
	}

#if defined(DE_PLATFORM_WINDOWS)
	struct Child
	{
		PROCESS_INFORMATION Info = {};

		bool Start(const std::filesystem::path& file)
		{
			std::wstring commandLine = L"\"" + Platform::GetExecutablePath().wstring() + L"\" --save-writer=\"" + file.wstring() + L"\"";
			STARTUPINFOW startup = { sizeof(startup) };
			return CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &Info) != 0;
		}

		void Kill()
		{
			TerminateProcess(Info.hProcess, 9);
			WaitForSingleObject(Info.hProcess, INFINITE);
			CloseHandle(Info.hThread);
			CloseHandle(Info.hProcess);
		}
	};
#else
	struct Child
	{
		pid_t Pid = -1;

		bool Start(const std::filesystem::path& file)
		{
			const std::string executable = Platform::GetExecutablePath().string();
			std::string argument = "--save-writer=" + file.string();
			char* argv[] = { const_cast<char*>(executable.c_str()), argument.data(), nullptr };
			return posix_spawn(&Pid, executable.c_str(), nullptr, nullptr, argv, environ) == 0;
		}

		void Kill()
		{
			kill(Pid, SIGKILL);
			int status = 0;
			waitpid(Pid, &status, 0);
		}
	};
#endif
}

TEST_SUITE("SaveData")
{
	TEST_CASE("values keep their type and read back with fallbacks")
	{
		CHECK(SaveData().IsNull());
		CHECK(SaveData(true).AsBool());
		CHECK(SaveData(true).AsInt(5) == 5);
		CHECK(SaveData(42).IsInt());
		CHECK(SaveData(42).AsInt() == 42);
		CHECK(SaveData(42).AsDouble() == 42.0);
		CHECK(SaveData(uint8_t(200)).AsInt() == 200);
		CHECK(SaveData(2.5).IsFloat());
		CHECK(SaveData(2.5).AsInt(-1) == -1);
		CHECK(SaveData(3.0).AsInt() == 3);
		CHECK(SaveData(1e300).AsInt(-1) == -1);
		CHECK(SaveData("text").AsString() == "text");
		CHECK(SaveData("text").AsFloat(1.0f) == 1.0f);
		CHECK(SaveData(std::string("x")).IsString());
		CHECK(SaveData(7).AsString("none") == "none");
		CHECK(SaveData(glm::vec2(1.0f, 2.0f)).AsVec2() == glm::vec2(1.0f, 2.0f));
		CHECK(SaveData(glm::vec2(1.0f, 2.0f)).AsVec3(glm::vec3(9.0f)) == glm::vec3(9.0f));
		CHECK(SaveData(glm::vec4(1.0f, 2.0f, 3.0f, 4.0f)).AsVec3() == glm::vec3(1.0f, 2.0f, 3.0f));
		CHECK(SaveData(glm::ivec3(1, -2, 3)).AsIVec3() == glm::ivec3(1, -2, 3));
		CHECK(SaveData(glm::vec2(1.5f, 2.0f)).AsIVec2(glm::ivec2(-1)) == glm::ivec2(-1));

		const glm::quat q = glm::angleAxis(0.7f, glm::normalize(glm::vec3(1.0f, 2.0f, 3.0f)));
		const glm::quat read = SaveData(q).AsQuat();
		CHECK(std::abs(glm::dot(read, q) - 1.0f) < 1e-6f);
		CHECK(SaveData(glm::vec4(0.0f)).AsQuat() == glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
	}

	TEST_CASE("Get reads members as the asked type")
	{
		SaveData data;
		data["volume"] = 0.5;
		data["count"] = 300;
		data["mode"] = Difficulty::Hard;
		data["name"] = "x";

		CHECK(data.Get("volume", 1.0f) == 0.5f);
		CHECK(data.Get("count", 0) == 300);
		CHECK(data.Get<uint8_t>("count", 7) == 7);
		CHECK(data.Get("mode", Difficulty::Easy) == Difficulty::Hard);
		CHECK(data.Get("name", std::string("y")) == "x");
		CHECK(data.Get("name", 4) == 4);
		CHECK(data.Get("missing", true) == true);
		CHECK(data.Get("volume", false) == false);
	}

	TEST_CASE("objects keep their keys sorted and unique")
	{
		SaveData data;
		data["b"] = 2;
		data["a"] = 1;
		data["c"] = 3;
		data["b"] = 20;
		REQUIRE(data.IsObject());
		REQUIRE(data.Size() == 3);
		CHECK(data.GetKey(0) == "a");
		CHECK(data.GetKey(1) == "b");
		CHECK(data.GetKey(2) == "c");
		CHECK(data.GetKey(3).empty());
		CHECK(data.At(1).AsInt() == 20);
		CHECK(data.Contains("c"));
		CHECK(data.Remove("c"));
		CHECK_FALSE(data.Remove("c"));
		CHECK_FALSE(data.Contains("c"));

		const SaveData& constant = data;
		CHECK(constant["missing"].IsNull());
		CHECK(constant.At(10).IsNull());
		CHECK(data.Size() == 2);
	}

	TEST_CASE("arrays index by position")
	{
		SaveData data;
		data.Push(1);
		data.Push("two");
		REQUIRE(data.IsArray());
		CHECK(data[0].AsInt() == 1);
		CHECK(data[1].AsString() == "two");
		data.Clear();
		CHECK(data.IsArray());
		CHECK(data.IsEmpty());
	}

	TEST_CASE("JSON round-trips every type exactly")
	{
		const SaveData data = MakeSample();
		for (bool pretty : { true, false })
		{
			const std::string json = data.ToJson(pretty);
			std::string error;
			const std::optional<SaveData> read = SaveData::FromJson(json, &error);
			REQUIRE_MESSAGE(read.has_value(), error);
			CHECK(*read == data);
			CHECK(read->ToJson(pretty) == json);
		}

		CHECK(SaveData::FromJson(SaveData(1.0).ToJson())->IsFloat());
		CHECK(SaveData::FromJson(SaveData(0.1f).ToJson())->AsFloat() == 0.1f);
		CHECK(SaveData::FromJson("18446744073709551615")->IsFloat());
	}

	TEST_CASE("non-finite floats are written as null")
	{
		SaveData data;
		data["nan"] = std::numeric_limits<double>::quiet_NaN();
		data["inf"] = std::numeric_limits<float>::infinity();
		const std::optional<SaveData> read = SaveData::FromJson(data.ToJson());
		REQUIRE(read.has_value());
		CHECK((*read)["nan"].IsNull());
		CHECK((*read)["inf"].IsNull());
	}

	TEST_CASE("bad JSON is rejected with a reason")
	{
		std::string error;
		CHECK_FALSE(SaveData::FromJson("{\"a\": [1, 2", &error).has_value());
		CHECK_FALSE(error.empty());
		CHECK_FALSE(SaveData::FromJson("", &error).has_value());
		CHECK_FALSE(SaveData::FromJson("{} trailing", &error).has_value());

		const std::string deep = std::string(300, '[') + std::string(300, ']');
		CHECK_FALSE(SaveData::FromJson(deep, &error).has_value());
		CHECK(error.find("256") != std::string::npos);
		const std::string shallow = std::string(200, '[') + std::string(200, ']');
		CHECK(SaveData::FromJson(shallow).has_value());
	}

	TEST_CASE("files keep the previous write as .bak and fall back to it")
	{
		TempDirectory directory("files");
		const std::filesystem::path path = directory.Path / "nested" / "slot.json";
		std::filesystem::path backup = path;
		backup += ".bak";

		std::string error;
		CHECK_FALSE(SaveData::ReadFile(path, &error).has_value());

		SaveData first;
		first["generation"] = 1;
		REQUIRE(first.WriteFile(path));
		CHECK_FALSE(std::filesystem::exists(backup));

		SaveData second;
		second["generation"] = 2;
		REQUIRE(second.WriteFile(path));
		REQUIRE(std::filesystem::exists(backup));

		std::filesystem::path source;
		CHECK(SaveData::ReadFile(path, nullptr, &source) == second);
		CHECK(source == path);

		WriteText(path, "{\"generation\": 3");
		CHECK(SaveData::ReadFile(path, nullptr, &source) == first);
		CHECK(source == backup);

		std::filesystem::remove(path);
		CHECK(SaveData::ReadFile(path) == first);

		WriteText(backup, "garbage");
		CHECK_FALSE(SaveData::ReadFile(path, &error).has_value());
		CHECK_FALSE(error.empty());
	}

	TEST_CASE("slots validate names and list newest first")
	{
		TempDirectory directory("slots");
#if defined(DE_PLATFORM_WINDOWS)
		const wchar_t* variable = L"LOCALAPPDATA";
		const wchar_t* previousValue = _wgetenv(variable);
		const std::wstring previous = previousValue ? previousValue : L"";
		_wputenv_s(variable, directory.Path.c_str());
#else
		const char* variable = "XDG_DATA_HOME";
		const char* previousValue = std::getenv(variable);
		const std::string previous = previousValue ? previousValue : "";
		setenv(variable, directory.Path.c_str(), 1);
#endif

		CHECK(SaveSlots::IsValidName("slot_1-a"));
		CHECK_FALSE(SaveSlots::IsValidName(""));
		CHECK_FALSE(SaveSlots::IsValidName("../escape"));
		CHECK_FALSE(SaveSlots::IsValidName("a.b"));
		CHECK_FALSE(SaveSlots::IsValidName(std::string(65, 'a')));
		CHECK(SaveSlots::GetDirectory().parent_path().parent_path() == directory.Path);

		CHECK(SaveSlots::List().empty());
		CHECK_FALSE(SaveSlots::Write("bad name", SaveData(1)));

		REQUIRE(SaveSlots::Write("older", SaveData(1)));
		REQUIRE(SaveSlots::Write("newer", SaveData(2)));
		REQUIRE(SaveSlots::Write("newer", SaveData(3)));
		std::filesystem::last_write_time(SaveSlots::GetPath("older"), std::filesystem::file_time_type::clock::now() - std::chrono::hours(1));

		const std::vector<SaveSlotInfo> slots = SaveSlots::List();
		REQUIRE(slots.size() == 2);
		CHECK(slots[0].Name == "newer");
		CHECK(slots[1].Name == "older");
		CHECK(SaveSlots::Read("newer")->AsInt() == 3);

		CHECK(SaveSlots::Exists("newer"));
		CHECK(SaveSlots::Delete("newer"));
		CHECK_FALSE(SaveSlots::Exists("newer"));
		CHECK(SaveSlots::List().size() == 1);

#if defined(DE_PLATFORM_WINDOWS)
		_wputenv_s(variable, previous.c_str());
#else
		if (previousValue)
			setenv(variable, previous.c_str(), 1);
		else
			unsetenv(variable);
#endif
	}

	TEST_CASE("a write killed at any moment leaves a complete save")
	{
		TempDirectory directory("kill");
		const std::filesystem::path path = directory.Path / "slot.json";
		REQUIRE(Test::MakeWriterSave(0).WriteFile(path));

		std::mt19937 random(1234);
		std::uniform_int_distribution<int> delayMs(20, 150);
		int64_t newest = 0;
		for (int run = 0; run < 16; run++)
		{
			Child child;
			REQUIRE(child.Start(path));
			std::this_thread::sleep_for(std::chrono::milliseconds(delayMs(random)));
			child.Kill();

			std::string error;
			const std::optional<SaveData> data = SaveData::ReadFile(path, &error);
			REQUIRE_MESSAGE(data.has_value(), error);
			const int64_t generation = (*data)["generation"].AsInt(-1);
			REQUIRE(generation >= 0);
			CHECK(*data == Test::MakeWriterSave(generation));
			newest = (std::max)(newest, generation);
		}
		INFO("generations written: ", newest);
		CHECK(newest > 0);
	}
}
