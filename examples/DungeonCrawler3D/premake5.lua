project "DungeonCrawler3D"
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

	filter "configurations:Debug or configurations:Debug-AS"
		symbols "On"
		defines { "DE_DEBUG" }

	filter "configurations:Release"
		optimize "On"
		defines { "DE_RELEASE" }

	filter "configurations:Distribution"
		kind "WindowedApp"
		optimize "On"
		symbols "Off"
		defines { "DE_DISTRIBUTION" }
