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
		"_CRT_SECURE_NO_WARNINGS",
        "VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1", 
        "_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING"
	}

    filter "action:vs*"
        --sanitize { "Address" }
        --flags { "NoRuntimeChecks", "NoIncrementalLink" }

        filter "language:C++ or language:C"
		    architecture "x86_64"

	    --filter "system:windows"
		--    buildoptions { "/EHsc", "/Zc:preprocessor", "/Zc:__cplusplus" }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Grab Vulkan SDK path
VULKAN_SDK = os.getenv("VULKAN_SDK")

IncludeDir = {}
IncludeDir['spdlog'] = "%{wks.location}/DingoEngine/vendor/spdlog/include";
IncludeDir['glfw'] = "%{wks.location}/DingoEngine/vendor/glfw/include";
IncludeDir['nvrhi'] = "%{wks.location}/DingoEngine/vendor/nvrhi/include";
IncludeDir['glm'] = "%{wks.location}/DingoEngine/vendor/glm";
IncludeDir['vulkan'] = "%{VULKAN_SDK}/Include";

LibraryDir = {}
LibraryDir['vulkan'] = "%{VULKAN_SDK}/lib";

Library = {}
Library['vulkan'] = "%{LibraryDir.vulkan}/vulkan-1.lib";

group "Engine"
include "DingoEngine"
group ""

group "Dependencies"
    include "DingoEngine/Vendor/spdlog"
    include "DingoEngine/vendor/glfw"
    include "DingoEngine/vendor/nvrhi"
group ""

include "Sandbox"