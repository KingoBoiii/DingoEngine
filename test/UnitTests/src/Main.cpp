#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <DingoEngine/Log.h>

#include "SaveWriter.h"

#include <string_view>

int main(int argc, char** argv)
{
	Dingo::Log::Initialize();

	constexpr std::string_view k_SaveWriter = "--save-writer=";
	if (argc == 2 && std::string_view(argv[1]).starts_with(k_SaveWriter))
		Dingo::Test::RunSaveWriter(std::filesystem::path(std::string_view(argv[1]).substr(k_SaveWriter.size())));

	doctest::Context context(argc, argv);
	const int result = context.run();

	Dingo::Log::Shutdown();
	return result;
}
