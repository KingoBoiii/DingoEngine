include "./vendor/premake/solution_items.lua"

workspace "DingoEngine"
    configurations { "Debug", "Debug-ASan", "Release" }
    startproject "Dingo-TestFramework"
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
IncludeDir['imgui'] = "%{wks.location}/vendor/imgui";
IncludeDir["msdfgen"] = "%{wks.location}/vendor/msdf-atlas-gen/msdfgen"
IncludeDir["msdf_atlas_gen"] = "%{wks.location}/vendor/msdf-atlas-gen/msdf-atlas-gen"
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


group "Dependencies"
    include "Vendor/spdlog"
    include "vendor/glfw"
    include "vendor/nvrhi"
    include "vendor/imgui"
	--include "vendor/msdf-atlas-gen"
group ""

group "Engine"
    project "DingoEngine"
		kind "StaticLib"
		language "C++"
		cppdialect "C++20"
		staticruntime "off"

		targetdir ("%{wks.location}/build/bin/" .. outputdir .. "/%{prj.name}")
		objdir ("%{wks.location}/build/bin-int/" .. outputdir .. "/%{prj.name}")

		pchheader "depch.h"
		pchsource "src/depch.cpp"

		files {
			"include/**.h",
			"include/**.hpp",
			"include/**.inl",
			"src/**.h",
			"src/**.c",
			"src/**.hpp",
			"src/**.cpp"
		}
	
		includedirs { 
			"include", 
			"src",
			"%{IncludeDir.spdlog}",
			"%{IncludeDir.glfw}",
			"%{IncludeDir.vulkan}",
			"%{IncludeDir.nvrhi}",
			"%{IncludeDir.glm}",
			"%{IncludeDir.stb}",
			"%{IncludeDir.imgui}",
			"%{IncludeDir.msdfgen}",
			"%{IncludeDir.msdf_atlas_gen}"
		}

		links {
			"spdlog",
			"glfw",
			"%{Library.vulkan}",
			"nvrhi",
			"imgui"
		}

		defines {
			"GLFW_INCLUDE_NONE"
		}

		buildoptions {
			"/utf-8"
		}

		filter "system:windows"
			systemversion "latest"
			defines { "DE_PLATFORM_WINDOWS", }

			links {
				"%{Library.WinSock}",
				"%{Library.WinMM}",
				"%{Library.WinVersion}",
				"%{Library.BCrypt}",
			}

		filter "system:linux"
			defines { "DE_PLATFORM_LINUX" }

		filter "configurations:Debug or configurations:Debug-AS"
			symbols "On"
			defines { "DE_DEBUG" }

			links {
				"%{Library.ShaderC_Debug}",
				"%{Library.SPIRV_Cross_Debug}",
				"%{Library.SPIRV_Cross_GLSL_Debug}"
			}

		filter "configurations:Release"
			optimize "On"
			defines { "DE_RELEASE" }

			links {
				"%{Library.ShaderC_Release}",
				"%{Library.SPIRV_Cross_Release}",
				"%{Library.SPIRV_Cross_GLSL_Release}"
			}
group ""

include "test"

group "Examples"
    include "examples/Sandbox"
group ""
