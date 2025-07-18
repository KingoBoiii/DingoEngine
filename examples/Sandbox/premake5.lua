project "Sandbox"
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
        "%{wks.location}/DingoEngine/include",
        "src",
        "%{IncludeDir.glm}",
        "%{IncludeDir.imgui}",

        "%{IncludeDir.nvrhi}" -- TODO: Client apps should not depend on this...
    }

    links { 
		"DingoEngine"
	}

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