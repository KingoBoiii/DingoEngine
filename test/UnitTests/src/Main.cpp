#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <DingoEngine/Log.h>

int main(int argc, char** argv)
{
	Dingo::Log::Initialize();

	doctest::Context context(argc, argv);
	const int result = context.run();

	Dingo::Log::Shutdown();
	return result;
}
