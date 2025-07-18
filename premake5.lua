include "./vendor/premake/solution_items.lua"

workspace "DingoEngine"
    configurations { "Debug", "Debug-ASan", "Release" }
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

        filter "configurations:Debug-ASan"
            symbols "On"
            editandcontinue "Off"
            buildoptions { "-fsanitize=address" }
            linkoptions { "-fsanitize=address" }

	    --filter "system:windows"
		--    buildoptions { "/EHsc", "/Zc:preprocessor", "/Zc:__cplusplus" }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Grab Vulkan SDK path
VULKAN_SDK = os.getenv("VULKAN_SDK")

IncludeDir = {}
IncludeDir['spdlog'] = "%{wks.location}/vendor/spdlog/include";
IncludeDir['glfw'] = "%{wks.location}/vendor/glfw/include";
IncludeDir['nvrhi'] = "%{wks.location}/vendor/nvrhi/include";
IncludeDir['glm'] = "%{wks.location}/vendor/glm";
IncludeDir['stb'] = "%{wks.location}/vendor/stb/include";
IncludeDir['imgui'] = "%{wks.location}/DingoEngine/vendor/imgui";
IncludeDir['vulkan'] = "%{VULKAN_SDK}/Include";

LibraryDir = {}
LibraryDir['vulkan'] = "%{VULKAN_SDK}/lib";

Library = {}
Library['vulkan'] = "%{LibraryDir.vulkan}/vulkan-1.lib";

Library["ShaderC_Debug"] = "%{LibraryDir.vulkan}/shaderc_sharedd.lib"
Library["SPIRV_Cross_Debug"] = "%{LibraryDir.vulkan}/spirv-cross-cored.lib"
Library["SPIRV_Cross_GLSL_Debug"] = "%{LibraryDir.vulkan}/spirv-cross-glsld.lib"
Library["SPIRV_Tools_Debug"] = "%{LibraryDir.vulkan}/SPIRV-Toolsd.lib"

Library["ShaderC_Release"] = "%{LibraryDir.vulkan}/shaderc_shared.lib"
Library["SPIRV_Cross_Release"] = "%{LibraryDir.vulkan}/spirv-cross-core.lib"
Library["SPIRV_Cross_GLSL_Release"] = "%{LibraryDir.vulkan}/spirv-cross-glsl.lib"

-- Windows
Library["WinSock"] = "Ws2_32.lib"
Library["WinMM"] = "Winmm.lib"
Library["WinVersion"] = "Version.lib"
Library["BCrypt"] = "Bcrypt.lib"

group "Engine"
    include "DingoEngine"
group ""

group "Dependencies"
    include "Vendor/spdlog"
    include "vendor/glfw"
    include "vendor/nvrhi"
    include "DingoEngine/vendor/imgui"
group ""

include "Sandbox"