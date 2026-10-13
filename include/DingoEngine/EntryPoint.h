#pragma once
#include "DingoEngine/Core/Application.h"

#if DE_DISTRIBUTION && DE_PLATFORM_WINDOWS

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
	return Dingo::Internal::Main(__argc, __argv);
}

#else

int main(int argc, char** argv)
{
	return Dingo::Internal::Main(argc, argv);
}

#endif
