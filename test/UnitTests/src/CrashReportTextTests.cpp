#include <doctest/doctest.h>

#include "DingoEngine/Core/CrashReportText.h"

#include <string>
#include <vector>

using namespace Dingo::Internal::CrashReportText;

TEST_SUITE("CrashReportText")
{
	TEST_CASE("spdlog levels map to breadcrumbs below Error and events from Error up")
	{
		CHECK(LevelFromSpdlog(0) == ReportLevel::Debug);
		CHECK(LevelFromSpdlog(1) == ReportLevel::Debug);
		CHECK(LevelFromSpdlog(2) == ReportLevel::Info);
		CHECK(LevelFromSpdlog(3) == ReportLevel::Warning);
		CHECK(LevelFromSpdlog(4) == ReportLevel::Error);
		CHECK(LevelFromSpdlog(5) == ReportLevel::Fatal);

		CHECK_FALSE(IsEvent(ReportLevel::Debug));
		CHECK_FALSE(IsEvent(ReportLevel::Info));
		CHECK_FALSE(IsEvent(ReportLevel::Warning));
		CHECK(IsEvent(ReportLevel::Error));
		CHECK(IsEvent(ReportLevel::Fatal));

		CHECK(std::string(LevelName(ReportLevel::Warning)) == "warning");
		CHECK(std::string(LevelName(ReportLevel::Fatal)) == "fatal");
	}

	TEST_CASE("the release names the game and the engine version")
	{
		CHECK(MakeRelease("Hearthvale", 1, 1, 0, 0) == "Hearthvale@1.1.0");
		CHECK(MakeRelease("Hearthvale", 1, 1, 0, 42) == "Hearthvale@1.1.0+42");
		CHECK(MakeRelease("Studio/Game\tX", 2, 0, 3, 0) == "Studio-Game-X@2.0.3");
		CHECK(MakeRelease("", 1, 0, 0, 0) == "Dingo@1.0.0");
	}

	TEST_CASE("the home folder becomes ~ in text, with either slash")
	{
		const HomeScrubber scrubber("C:\\Users\\Anna", u"C:\\Users\\Anna", true);
		CHECK(scrubber.Apply("Log file: C:\\Users\\Anna\\AppData\\Local\\Game\\logs\\Dingo.log") == "Log file: ~\\AppData\\Local\\Game\\logs\\Dingo.log");
		CHECK(scrubber.Apply("c:/users/anna/saves and C:\\Users\\Anna") == "~/saves and ~");
		CHECK(scrubber.Apply("C:\\Users\\Annabel\\x") == "C:\\Users\\Annabel\\x");
		CHECK(scrubber.Apply("no path here") == "no path here");

		const HomeScrubber posix("/home/anna/", u"/home/anna/", false);
		CHECK(posix.Apply("/home/anna/.local/share/Game/logs") == "~/.local/share/Game/logs");
		CHECK(posix.Apply("/home/Anna/x") == "/home/Anna/x");
	}

	TEST_CASE("a home folder too short to be one scrubs nothing")
	{
		const HomeScrubber root("/", u"/", false);
		CHECK_FALSE(root.IsActive());
		CHECK(root.Apply("/usr/bin") == "/usr/bin");
	}

	TEST_CASE("binary data keeps its length with the user name masked, in UTF-8 and UTF-16")
	{
		const HomeScrubber scrubber("C:\\Users\\Anna", u"C:\\Users\\Anna", true);
		const std::string narrow = "xxC:\\Users\\Anna\\Game.exe";
		const std::u16string wide = u"C:\\Users\\Anna\\Game.exe";
		std::vector<char> bytes(narrow.begin(), narrow.end());
		const size_t wideAt = bytes.size();
		for (char16_t unit : wide)
		{
			bytes.push_back(static_cast<char>(unit & 0xFF));
			bytes.push_back(static_cast<char>(unit >> 8));
		}
		const size_t size = bytes.size();

		scrubber.MaskBinary(bytes);

		REQUIRE(bytes.size() == size);
		CHECK(std::string(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(narrow.size())) == "xxC:\\Users\\____\\Game.exe");
		std::u16string maskedWide;
		for (size_t i = wideAt; i + 1 < bytes.size(); i += 2)
			maskedWide.push_back(static_cast<char16_t>(static_cast<uint8_t>(bytes[i]) | (static_cast<uint8_t>(bytes[i + 1]) << 8)));
		CHECK(maskedWide == u"C:\\Users\\____\\Game.exe");
	}

	TEST_CASE("a crash report is found with its minidump and the lines before it")
	{
		const std::string log =
			"[10:00:00] [info] Dingo: Log file: C:\\Users\\Anna\\logs\\Dingo.log\r\n"
			"[10:00:01] [warning] Game: [CrashDemo] Crashing on purpose\r\n"
			"\r\n"
			"*** Flappy Bird crashed: access violation writing 0x0\r\n"
			"*** Crash reporting: on\r\n"
			"*** Minidump: C:\\Users\\Anna\\logs\\Dingo-20261101-100001-42.dmp\r\n";

		const auto crash = FindCrash(log);
		REQUIRE(crash.has_value());
		CHECK(crash->Complete);
		CHECK(crash->ReportingOn);
		CHECK(crash->Header == "Flappy Bird crashed: access violation writing 0x0");
		CHECK(crash->MinidumpPath == "C:\\Users\\Anna\\logs\\Dingo-20261101-100001-42.dmp");
		CHECK(crash->Report.starts_with("*** Flappy Bird crashed:"));
		CHECK(crash->Report.find("*** Minidump: ") != std::string::npos);
		CHECK(crash->Tail.find("Crashing on purpose") != std::string::npos);
		CHECK(crash->Tail.find("crashed:") == std::string::npos);
	}

	TEST_CASE("a Linux backtrace report is found and its tail is limited")
	{
		std::string log;
		for (int i = 0; i < 300; ++i)
			log += "[10:00:00] [trace] Dingo: line " + std::to_string(i) + "\n";
		log += "\n*** Game crashed: SIGSEGV\n./Game(+0x1234)\n*** End of backtrace\n";

		const auto crash = FindCrash(log, 10);
		REQUIRE(crash.has_value());
		CHECK(crash->Header == "Game crashed: SIGSEGV");
		CHECK(crash->Complete);
		CHECK_FALSE(crash->ReportingOn);
		CHECK(crash->MinidumpPath.empty());
		CHECK(crash->Report.find("End of backtrace") != std::string::npos);
		CHECK(crash->Tail.find("line 299") != std::string::npos);
		CHECK(crash->Tail.find("line 290") == std::string::npos);
	}

	TEST_CASE("a report still being written is incomplete, and its key holds once it is done")
	{
		const std::string started = "[10:00:00] [info] Dingo: x\n\n*** Game crashed: access violation\n";
		const auto partial = FindCrash(started);
		REQUIRE(partial.has_value());
		CHECK_FALSE(partial->Complete);

		const auto done = FindCrash(started + "*** Crash reporting: on\n*** Minidump: D:\\d.dmp\n");
		const auto doneAgain = FindCrash(started + "*** Crash reporting: on\n*** Minidump: D:\\d.dmp\n[10:00:09] [info] late line\n");
		REQUIRE(done.has_value());
		REQUIRE(doneAgain.has_value());
		CHECK(done->Complete);
		CHECK(CrashKey(*done).size() == 16);
		CHECK(CrashKey(*done) != CrashKey(*partial));
		CHECK(CrashKey(*done) == CrashKey(*doneAgain));

		const auto other = FindCrash("[10:00:01] [info] Dingo: y\n\n*** Game crashed: access violation\n*** Minidump: D:\\d.dmp\n");
		REQUIRE(other.has_value());
		CHECK(CrashKey(*done) != CrashKey(*other));
	}

	TEST_CASE("truncation never splits a UTF-8 character")
	{
		const std::string text = "ab\xC3\xA6" "c";
		CHECK(TruncateUtf8(text, 10) == text);
		CHECK(TruncateUtf8(text, 4) == "ab\xC3\xA6");
		CHECK(TruncateUtf8(text, 3) == "ab");
		CHECK(TruncateUtf8(text, 0).empty());
	}

	TEST_CASE("a log without a crash report has none")
	{
		CHECK_FALSE(FindCrash("[10:00:00] [error] Dingo: something failed\n*** Fatal error: x (a.cpp:1)\n").has_value());
		CHECK_FALSE(FindCrash("").has_value());
	}

	TEST_CASE("each message is an event once, and 25 at most")
	{
		EventLimiter limiter;
		CHECK(limiter.Allow("a"));
		CHECK_FALSE(limiter.Allow("a"));
		for (int i = 1; i < 25; ++i)
			CHECK(limiter.Allow("message " + std::to_string(i)));
		CHECK_FALSE(limiter.Allow("one too many"));
	}
}
