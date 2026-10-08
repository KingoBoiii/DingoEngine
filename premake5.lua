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
            -- The Vulkan SDK's shaderc / SPIRV prebuilts are not ASan builds, and the STL's
            -- container annotations must agree across every object or the link fails with
            -- LNK2038. cl embeds /INFERASANLIBS, so the linker needs no ASan flag of its own.
            defines { "_DISABLE_STRING_ANNOTATION", "_DISABLE_VECTOR_ANNOTATION" }

        -- GCC has no /INFERASANLIBS: the ASan runtime only links in through the flag.
        filter { "system:linux", "configurations:Debug-ASan" }
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

-- The same set for the Linux libDingoEngine.a, with the static assimp as well (Windows links
-- assimp's import library and ships its DLLs).
local function linuxVendorLib(rel, name)
    return '"' .. path.join(_MAIN_SCRIPT_DIR, "vendor", rel, "bin") .. '/' .. outputdir .. '/' .. name .. '/lib' .. name .. '.a"'
end

LinuxBundledVendorLibs = table.concat({
    linuxVendorLib("spdlog", "spdlog"),
    linuxVendorLib("glfw", "GLFW"),
    linuxVendorLib("nvrhi", "NVRHI"),
    linuxVendorLib("nvrhi", "NVRHI-Vulkan"),
    linuxVendorLib("imgui", "ImGui"),
    linuxVendorLib("msdf-atlas-gen", "msdf-atlas-gen"),
    linuxVendorLib("msdf-atlas-gen/msdfgen", "msdfgen"),
    linuxVendorLib("msdf-atlas-gen/msdfgen/freetype", "freetype"),
    linuxVendorLib("box2d", "box2d"),
    linuxVendorLib("JoltPhysics", "Jolt"),
    '"' .. path.join(_MAIN_SCRIPT_DIR, "vendor/assimp/lib/linux-x86_64/libassimp.a") .. '"',
}, " ")

-- On Windows, every exe that links the engine also loads Assimp's DLLs (and their
-- zlib / pugixml / poly2tri deps) at startup — plus, in Debug-ASan, the ASan runtime —
-- so they have to sit next to the binary or it dies with STATUS_DLL_NOT_FOUND before
-- main. On Linux, make links only what a project names, not the engine's own
-- dependencies as MSBuild does, so every exe names them all; the link group makes
-- their order irrelevant. Call this once per project;
-- it appends the per-configuration settings and clears the filter again.
-- The source dir is baked to an absolute path at generation time because
-- %{wks.location} expands to empty inside an included project's postbuild scope.
function copyAssimpRuntime()
    local debugBin   = path.join(_MAIN_SCRIPT_DIR, "vendor/assimp/bin/debug")
    local releaseBin = path.join(_MAIN_SCRIPT_DIR, "vendor/assimp/bin/release")

    filter { "system:windows", "configurations:Debug or configurations:Debug-ASan" }
        postbuildcommands { '{COPY} "' .. debugBin .. '" "%{cfg.targetdir}"' }

    filter { "system:windows", "configurations:Release or configurations:Distribution" }
        postbuildcommands { '{COPY} "' .. releaseBin .. '" "%{cfg.targetdir}"' }

    filter { "system:windows", "configurations:Debug-ASan" }
        postbuildcommands { '{COPY} "$(VCToolsInstallDir)bin\\Hostx64\\x64\\clang_rt.asan_dynamic-x86_64.dll" "%{cfg.targetdir}"' }

    filter { "system:linux", "kind:ConsoleApp or WindowedApp" }
        linkgroups "On"
        libdirs { "%{LibraryDir.vulkan}", "%{LibraryDir.assimp}" }
        links {
            "DingoEngine", "spdlog", "GLFW", "NVRHI", "NVRHI-Vulkan", "ImGui",
            "msdf-atlas-gen", "msdfgen", "freetype", "box2d", "Jolt",
            "shaderc_combined", "spirv-cross-hlsl", "spirv-cross-glsl", "spirv-cross-core",
            "assimp", "z", "dl", "pthread"
        }

    filter {}
end

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
IncludeDir['vulkan'] = "%{VULKAN_SDK}/include";
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

-- The Linux SDK has no debug-suffixed libraries, and vendor/assimp only holds Windows
-- binaries: Linux links a static assimp built into vendor/assimp/lib/linux-x86_64.
if os.istarget("linux") then
    LibraryDir['assimp'] = "%{wks.location}/vendor/assimp/lib/linux-x86_64"
    Library['assimp_Debug']   = "assimp"
    Library['assimp_Release'] = "assimp"
    for _, cfg in ipairs({ "Debug", "Release" }) do
        Library["ShaderC_" .. cfg]          = "shaderc_combined"
        Library["SPIRV_Cross_" .. cfg]      = "spirv-cross-core"
        Library["SPIRV_Cross_GLSL_" .. cfg] = "spirv-cross-glsl"
        Library["SPIRV_Cross_HLSL_" .. cfg] = "spirv-cross-hlsl"
    end
end

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

	-- Linux additions to the forks' own scripts, made here so vendor/ stays untouched.
	project "GLFW"
		filter "system:linux"
			files { "vendor/glfw/src/posix_module.c", "vendor/glfw/src/posix_poll.c" }
		filter {}

	project "NVRHI-Vulkan"
		filter "system:linux"
			includedirs { "%{VULKAN_SDK}/include" }
		filter {}

	-- gmake writes each makefile next to its project's script, inside the submodule, where it
	-- would overwrite FreeType's own tracked Makefile. Visual Studio's projects stay where they are.
	if _ACTION == "gmake" or _ACTION == "gmake2" then
		local vendorProjects = { "spdlog", "GLFW", "NVRHI", "NVRHI-Vulkan", "ImGui", "msdf-atlas-gen", "msdfgen", "freetype", "box2d", "Jolt" }
		if os.istarget("windows") then
			table.insert(vendorProjects, "NVRHI-D3D11")
			table.insert(vendorProjects, "NVRHI-D3D12")
		end
		for _, name in ipairs(vendorProjects) do
			project(name)
				location("build/make/" .. name)
		end
	end
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
			"src/**.cpp",
			"src/**/Shaders/*.glsl"
		}

		includedirs {
			"include",
			"src",
			"%{cfg.objdir}/Embedded",
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
			"nvrhi",
			"imgui",
			"msdf-atlas-gen",
			"box2d",
			"Jolt"
		}

		-- The SPDLOG_ pair must match vendor/spdlog's own defines, or Log.cpp compiles spdlog
		-- header-only with the bundled fmt and the binary carries two incompatible copies of it.
		defines {
			"GLFW_INCLUDE_NONE",
			"SPDLOG_COMPILED_LIB",
			"SPDLOG_USE_STD_FORMAT"
		}

		filter "files:src/**/Shaders/*.glsl"
			buildmessage "Embedding %{file.name}"
			buildcommands {
				'"' .. _PREMAKE_COMMAND .. '" --file="' .. path.join(_MAIN_SCRIPT_DIR, "scripts/embed.lua")
					.. '" --input="%{file.abspath}" --output="%{cfg.objdir}/Embedded/%{file.name}.inl" embed'
			}
			buildinputs { path.join(_MAIN_SCRIPT_DIR, "scripts/embed.lua") }
			buildoutputs { "%{cfg.objdir}/Embedded/%{file.name}.inl" }

		-- Debug loads engine shaders from the source tree so they hot-reload. Release and
		-- Distribution always run the embedded copy, so they never carry a build-machine path or pick
		-- up a source file the C++ wasn't built against. A published Debug lib still carries the
		-- builder's path, which falls back to the embedded copy wherever that path doesn't exist.
		filter "configurations:Debug or configurations:Debug-ASan"
			defines { 'DE_ENGINE_SHADER_DIR="' .. path.join(_MAIN_SCRIPT_DIR, "src/DingoEngine/Graphics/Shaders") .. '"' }

		filter "system:windows"
			systemversion "latest"
			defines { "DE_PLATFORM_WINDOWS", "GLFW_EXPOSE_NATIVE_WIN32" }
			buildoptions { "/utf-8" }

			includedirs {
				"%{IncludeDir.dx_headers}"
			}

			links {
				"%{Library.vulkan}",
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
			removefiles { "src/DingoEngine/Graphics/NVRHI/DirectX11/**", "src/DingoEngine/Graphics/NVRHI/DirectX12/**" }

			-- The distributable libDingoEngine.a under build/dist, like Windows' DingoEngine.lib.
			-- Consumers add only the Vulkan SDK's shaderc and SPIRV-Cross, zlib, dl and pthread.
			postbuildcommands {
				'sh "' .. path.join(_MAIN_SCRIPT_DIR, "scripts/merge-static-libs.sh") .. '" "'
					.. path.join(_MAIN_SCRIPT_DIR, "build/dist") .. '/' .. outputdir .. '/libDingoEngine.a" "%{cfg.buildtarget.abspath}" '
					.. LinuxBundledVendorLibs,
			}

		filter "configurations:Debug or configurations:Debug-ASan"
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

		-- The release ZIP is packed from this target dir, so consumers of the
		-- prebuilt lib get the Assimp DLLs from here too.
		copyAssimpRuntime()

group ""

include "test"

group "Examples"
    include "examples/ArenaShooter"
    include "examples/FlappyBird"
    include "examples/Breakout3D"
    include "examples/DungeonCrawler"
    include "examples/SpaceInvaders"
    include "examples/AngryBirds"
    include "examples/DungeonCrawler3D"
    include "examples/EchoVault"
    include "examples/Candlewick"
    include "examples/Marionette"
group ""
