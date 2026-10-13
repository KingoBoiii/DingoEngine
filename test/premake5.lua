project "Dingo-TestFramework"
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
        "%{wks.location}/src",
        "%{IncludeDir.glm}",
        "%{IncludeDir.imgui}"
    }

    links { 
		"DingoEngine"
	}

	filter "system:windows"
		systemversion "latest"
		defines { "DE_PLATFORM_WINDOWS", }

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
		vectorextensions "AVX2"
		defines { "DE_DISTRIBUTION" }

	copyAssimpRuntime()
	exeMetadata {
		Name = "Dingo Test Framework",
		Description = "Interactive graphics, renderer and asset tests for Dingo Engine.",
		Company = "Codevyn Games",
		Copyright = "Copyright (c) 2024 Victor Hansen. MIT License; see THIRD-PARTY-NOTICES.md.",
	}
