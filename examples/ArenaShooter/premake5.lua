project "ArenaShooter"
    kind "ConsoleApp"

    targetdir ("%{wks.location}/build/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/build/bin-int/" .. outputdir .. "/%{prj.name}")

	files {
		"src/**.h",
		"src/**.c",
		"src/**.hpp",
		"src/**.cpp"
	}

	includedirs {
        "%{wks.location}/include",
        "src",
        "%{IncludeDir.glm}"
    }

    links {
		"DingoEngine"
	}

	filter "system:windows"
		systemversion "latest"
		-- NOMINMAX: DingoEngine's public headers transitively include <Windows.h>,
		-- whose min/max macros otherwise clobber std::min / std::max in game code.
		defines { "DE_PLATFORM_WINDOWS", "NOMINMAX" }

	filter "system:linux"
		defines { "DE_PLATFORM_LINUX" }

	-- The engine links Assimp; the exe needs the Assimp DLL next to it at runtime.
	-- (%{wks.location} resolves empty inside this included project's postbuild scope,
	-- so the source dir is baked to an absolute path here at generation time instead.)
	local assimpDebugBin   = path.join(_MAIN_SCRIPT_DIR, "vendor/assimp/bin/debug")
	local assimpReleaseBin = path.join(_MAIN_SCRIPT_DIR, "vendor/assimp/bin/release")

	filter "configurations:Debug or configurations:Debug-AS"
		symbols "On"
		defines { "DE_DEBUG" }
		postbuildcommands {
			'{COPY} "' .. assimpDebugBin .. '" "%{cfg.targetdir}"'
		}

	filter "configurations:Release"
		optimize "On"
		defines { "DE_RELEASE" }
		postbuildcommands {
			'{COPY} "' .. assimpReleaseBin .. '" "%{cfg.targetdir}"'
		}

	filter "configurations:Distribution"
		kind "WindowedApp"
		optimize "On"
		symbols "Off"
		defines { "DE_DISTRIBUTION" }
		postbuildcommands {
			'{COPY} "' .. assimpReleaseBin .. '" "%{cfg.targetdir}"'
		}
