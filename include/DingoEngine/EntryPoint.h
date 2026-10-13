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

/// The program's entry point in a Windows Distribution build (no console window): runs
/// Internal::Main, which calls CreateApplication. Include EntryPoint.h in exactly one source file.
int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
	return Dingo::Internal::Main(__argc, __argv);
}

#else

/// The program's entry point: runs Internal::Main, which calls the game's CreateApplication, runs
/// the application and, on RequestRestart, creates it again. Include EntryPoint.h in exactly one
/// source file; the game does not write main.
int main(int argc, char** argv)
{
	return Dingo::Internal::Main(argc, argv);
}

#endif
