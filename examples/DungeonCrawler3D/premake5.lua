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
		defines { "DE_PLATFORM_WINDOWS", "NOMINMAX" }

	filter "system:linux"
		defines { "DE_PLATFORM_LINUX" }

	filter "configurations:Debug or configurations:Debug-ASan"
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

	copyAssimpRuntime()
	exeMetadata {
		Name = "Dungeon Crawler 3D",
		Description = "Procedural 3D dungeons, an example made with Dingo Engine.",
		Company = "Codevyn Games",
		Copyright = "Copyright (c) 2024 Victor Hansen. MIT License; see THIRD-PARTY-NOTICES.md.",
	}
	embedPackKey()
