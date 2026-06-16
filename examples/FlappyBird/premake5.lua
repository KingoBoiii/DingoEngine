project "FlappyBird"
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
        "%{IncludeDir.glm}",
        "%{IncludeDir.imgui}"
    }

    links { 
		"DingoEngine"
	}

	filter "system:windows"
		systemversion "latest"
		defines { "DE_PLATFORM_WINDOWS", }

		-- Visual Studio build metadata
		files {
			"Dingo-FlappyBird.aps",
			"Dingo-FlappyBird.rc",
			"resource.h"
		}

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