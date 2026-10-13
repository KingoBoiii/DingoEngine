#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <DingoEngine/Log.h>

#include "SaveWriter.h"

#include <cstdlib>
#include <string_view>

int main(int argc, char** argv)
{
	Dingo::Log::Initialize();

	if (argc == 2 && std::string_view(argv[1]) == "--save-writer")
	{
#if defined(DE_PLATFORM_WINDOWS)
		const wchar_t* path = _wgetenv(Dingo::Test::k_SaveWriterVariableW);
#else
		const char* path = std::getenv(Dingo::Test::k_SaveWriterVariable);
#endif
		if (path)
			Dingo::Test::RunSaveWriter(std::filesystem::path(path));
		return 2;
	}

	doctest::Context context(argc, argv);
	const int result = context.run();

	Dingo::Log::Shutdown();
	return result;
}
