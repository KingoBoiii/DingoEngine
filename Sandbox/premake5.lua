project "Sandbox"
    kind "ConsoleApp"

    targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
	objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

	files {
		"src/**.h",
		"src/**.c",
		"src/**.hpp",
		"src/**.cpp"
	}
	
	includedirs {
        "src/",
        "../DingoEngine/include/"
    }

    links { "DingoEngine" }

	filter "system:windows"
		systemversion "latest"
		defines { "DE_PLATFORM_WINDOWS", }

	filter "system:linux"
		defines { "DE_PLATFORM_LINUX" }

	filter "configurations:Debug or configurations:Debug-AS"
		symbols "On"
		defines { "DE_DEBUG" }

	filter "configurations:Release"
		optimize "On"
		defines { "DE_RELEASE" }