include "./vendor/premake/solution_items.lua"

workspace "DingoEngine"
    configurations { "Debug", "Debug-ASan", "Release", "Distribution" }
    startproject "Dingo-TestFramework"
    language "C++"
	cppdialect "C++20"
	staticruntime "Off"
    architecture "x86_64"
    solution_items { ".editorconfig" }
    flags { "multiprocessorcompile" }

    -- C7: embed debug info in the objects instead of external vc*.pdb files,
    -- so the merged distributable DingoEngine.lib is debuggable on its own.
    -- Trade-off: no Edit-and-Continue.
    debugformat "c7"

    defines {
		"_CRT_SECURE_NO_WARNINGS",
        "VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1",
        "_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING",
        "GLM_FORCE_DEPTH_ZERO_TO_ONE"
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

	    filter "system:windows"
		    buildoptions { "/EHsc", "/Zc:preprocessor", "/Zc:__cplusplus" }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Every in-tree vendor static lib that gets merged into the distributable
-- DingoEngine.lib (see the lib.exe post-build step on the DingoEngine project).
-- Anchored on $(ProjectDir) (= repo root for DingoEngine) instead of
-- $(SolutionDir), which is undefined when a .vcxproj is built standalone.
local function vendorLib(rel, name)
    return '"$(ProjectDir)vendor/' .. rel .. '/bin/' .. outputdir .. '/' .. name .. '/' .. name .. '.lib"'
end

BundledVendorLibs = table.concat({
    vendorLib("spdlog", "spdlog"),
    vendorLib("glfw", "GLFW"),
    vendorLib("nvrhi", "NVRHI"),
    vendorLib("nvrhi", "NVRHI-Vulkan"),
    vendorLib("nvrhi", "NVRHI-D3D11"),
    vendorLib("nvrhi", "NVRHI-D3D12"),
    vendorLib("imgui", "ImGui"),
    vendorLib("msdf-atlas-gen", "msdf-atlas-gen"),
    vendorLib("msdf-atlas-gen/msdfgen", "msdfgen"),
    vendorLib("msdf-atlas-gen/msdfgen/freetype", "freetype"),
    vendorLib("box2d", "box2d"),
    vendorLib("JoltPhysics", "Jolt"),
}, " ")

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
IncludeDir['dx_headers'] = "%{wks.location}/vendor/nvrhi/thirdparty/DirectX-Headers/include";
IncludeDir['assimp'] = "%{wks.location}/vendor/assimp/include";
IncludeDir['entt'] = "%{wks.location}/vendor/entt/include";
IncludeDir['box2d'] = "%{wks.location}/vendor/box2d/include";
IncludeDir['jolt'] = "%{wks.location}/vendor/JoltPhysics";
IncludeDir['miniaudio'] = "%{wks.location}/vendor/miniaudio"; -- header-only (miniaudio.h + stb_vorbis.c), engine-only

LibraryDir = {}
LibraryDir['vulkan'] = "%{VULKAN_SDK}/lib";
LibraryDir['assimp'] = "%{wks.location}/vendor/assimp/lib";

Library = {}
Library['vulkan'] = "%{LibraryDir.vulkan}/vulkan-1.lib";

Library['assimp_Debug']   = "%{LibraryDir.assimp}/assimp-vc145-mtd.lib"
Library['assimp_Release'] = "%{LibraryDir.assimp}/assimp-vc145-mt.lib"

Library["ShaderC_Debug"] = "%{LibraryDir.vulkan}/shaderc_combinedd.lib" -- shaderc_sharedd
Library["SPIRV_Cross_Debug"] = "%{LibraryDir.vulkan}/spirv-cross-cored.lib"
Library["SPIRV_Cross_GLSL_Debug"] = "%{LibraryDir.vulkan}/spirv-cross-glsld.lib"
Library["SPIRV_Cross_HLSL_Debug"] = "%{LibraryDir.vulkan}/spirv-cross-hlsld.lib"
Library["SPIRV_Tools_Debug"] = "%{LibraryDir.vulkan}/SPIRV-Toolsd.lib"

Library["ShaderC_Release"] = "%{LibraryDir.vulkan}/shaderc_combined.lib" -- shaderc_shared
Library["SPIRV_Cross_Release"] = "%{LibraryDir.vulkan}/spirv-cross-core.lib"
Library["SPIRV_Cross_GLSL_Release"] = "%{LibraryDir.vulkan}/spirv-cross-glsl.lib"
Library["SPIRV_Cross_HLSL_Release"] = "%{LibraryDir.vulkan}/spirv-cross-hlsl.lib"

-- Windows
Library["WinSock"] = "Ws2_32.lib"
Library["WinMM"] = "Winmm.lib"
Library["WinVersion"] = "Version.lib"
Library["BCrypt"] = "Bcrypt.lib"
Library["D3D12"] = "d3d12.lib"
Library["D3D11"] = "d3d11.lib"
Library["DXGI"] = "dxgi.lib"
Library["DXGUID"] = "dxguid.lib"
Library["D3DCompiler"] = "d3dcompiler.lib"


group "Dependencies"
	include "vendor/spdlog"
	include "vendor/glfw"
	include "vendor/nvrhi"
	include "vendor/imgui"
	include "vendor/msdf-atlas-gen"
	include "vendor/box2d"
	include "vendor/JoltPhysics"
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
			"%{IncludeDir.msdf_atlas_gen}",
			"%{IncludeDir.assimp}",
			"%{IncludeDir.entt}",
			"%{IncludeDir.box2d}",
			"%{IncludeDir.jolt}",
			"%{IncludeDir.miniaudio}"
		}

		links {
			"spdlog",
			"glfw",
			"%{Library.vulkan}",
			"nvrhi",
			"imgui",
			"msdf-atlas-gen",
			"box2d",
			"Jolt"
		}

		defines {
			"GLFW_INCLUDE_NONE"
		}

		filter "system:windows"
			systemversion "latest"
			defines { "DE_PLATFORM_WINDOWS", "GLFW_EXPOSE_NATIVE_WIN32" }
			buildoptions { "/utf-8" }

			includedirs {
				"%{IncludeDir.dx_headers}"
			}

			links {
				"%{Library.WinSock}",
				"%{Library.WinMM}",
				"%{Library.WinVersion}",
				"%{Library.BCrypt}",
				"%{Library.D3D12}",
				"%{Library.D3D11}",
				"%{Library.DXGI}",
				"%{Library.DXGUID}",
				"%{Library.D3DCompiler}",
			}

			-- Merge the engine + every in-tree vendor static lib into one
			-- self-contained DingoEngine.lib under build/dist. lib.exe is
			-- resolved via MSBuild so this also works outside a VS dev shell.
			-- /IGNORE:4006: the NVRHI sub-libs share identical objects
			-- (validation-device.obj, dxgi-format.obj); duplicates are benign.
			postbuildcommands {
				'{MKDIR} "$(ProjectDir)build/dist/' .. outputdir .. '"',
				'"$(VCToolsInstallDir)bin\\Hostx64\\x64\\lib.exe" /NOLOGO /IGNORE:4006 /OUT:"$(ProjectDir)build/dist/'
					.. outputdir .. '/DingoEngine.lib" "$(TargetPath)" ' .. BundledVendorLibs,
			}

		filter "system:linux"
			defines { "DE_PLATFORM_LINUX" }
			buildoptions { "-Wno-changes-meaning" }

		filter "configurations:Debug or configurations:Debug-AS"
			runtime "Debug"
			symbols "On"
			defines { "DE_DEBUG" }

			links {
				"%{Library.ShaderC_Debug}",
				"%{Library.SPIRV_Cross_Debug}",
				"%{Library.SPIRV_Cross_GLSL_Debug}",
				"%{Library.SPIRV_Cross_HLSL_Debug}",
				"%{Library.assimp_Debug}",
			}

			postbuildcommands {
				"{COPY} %{wks.location}/vendor/assimp/bin/debug %{cfg.targetdir}"
			}

		filter "configurations:Release"
			runtime "Release"
			optimize "On"
			defines { "DE_RELEASE" }

			links {
				"%{Library.ShaderC_Release}",
				"%{Library.SPIRV_Cross_Release}",
				"%{Library.SPIRV_Cross_GLSL_Release}",
				"%{Library.SPIRV_Cross_HLSL_Release}",
				"%{Library.assimp_Release}",
			}

			postbuildcommands {
				"{COPY} %{wks.location}/vendor/assimp/bin/release %{cfg.targetdir}"
			}

		filter "configurations:Distribution"
			runtime "Release"
			optimize "On"
			symbols "Off"
			defines { "DE_DISTRIBUTION" }

			links {
				"%{Library.ShaderC_Release}",
				"%{Library.SPIRV_Cross_Release}",
				"%{Library.SPIRV_Cross_GLSL_Release}",
				"%{Library.SPIRV_Cross_HLSL_Release}",
				"%{Library.assimp_Release}",
			}

			postbuildcommands {
				"{COPY} %{wks.location}/vendor/assimp/bin/release %{cfg.targetdir}"
			}

group ""

include "test"

group "Examples"
    include "examples/FlappyBird"
    include "examples/Breakout3D"
    include "examples/DungeonCrawler"
    include "examples/SpaceInvaders"
    include "examples/AngryBirds"
    include "examples/DungeonCrawler3D"
    include "examples/EchoVault"
group ""
