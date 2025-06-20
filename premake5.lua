include "./vendor/premake/solution_items.lua"

workspace "DingoEngine"
    configurations { "Debug", "Release" }
    startproject "Sandbox"
    language "C++"
	cppdialect "C++20"
	staticruntime "Off"
    solution_items { ".editorconfig" }
    flags { "MultiProcessorCompile" }

    defines {
		"_CRT_SECURE_NO_WARNINGS"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

IncludeDir = {}
IncludeDir['glfw'] = "%{wks.location}/DingoEngine/vendor/glfw/include";

LibraryDir = {}
Library = {}

group "Engine"
include "DingoEngine"
group ""

group "Dependencies"
include "DingoEngine/vendor/glfw"
group ""

include "Sandbox"