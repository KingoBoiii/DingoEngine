#pragma once

namespace Dingo::Test
{

	// The child reads its folder from here rather than argv, which is ANSI on Windows.
	inline constexpr const char* k_LogWriterVariable = "DINGO_LOG_WRITER_DIR";
	inline constexpr const wchar_t* k_LogWriterVariableW = L"DINGO_LOG_WRITER_DIR";
	// Milliseconds since the system clock's epoch: the children open their logs together then.
	inline constexpr const char* k_LogWriterStartVariable = "DINGO_LOG_WRITER_START";
	inline constexpr const wchar_t* k_LogWriterStartVariableW = L"DINGO_LOG_WRITER_START";
	// Milliseconds the child keeps its log open between its start and end lines.
	inline constexpr const char* k_LogWriterHoldVariable = "DINGO_LOG_WRITER_HOLD";
	inline constexpr const wchar_t* k_LogWriterHoldVariableW = L"DINGO_LOG_WRITER_HOLD";

	// The child process of the concurrent-start test: opens a log in the folder, writes a start and
	// an end line, and returns 3 when the file Log::GetFilePath() names lost its start line.
	int RunLogWriter();

}
