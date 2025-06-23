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
        "src",
        "%{IncludeDir.spdlog}", -- TODO: Client apps should not depend on this...
        "%{IncludeDir.nvrhi}", -- TODO: Client apps should not depend on this...
        "%{wks.location}/DingoEngine/include"
    }

    links { 
		"DingoEngine"
	}

    buildoptions {
        "/utf-8"
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