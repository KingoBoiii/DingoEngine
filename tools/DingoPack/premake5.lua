project "DingoPack"
	kind "ConsoleApp"

	targetdir ("%{wks.location}/build/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/build/bin-int/" .. outputdir .. "/%{prj.name}")

	files {
		"src/**.h",
		"src/**.cpp"
	}

	-- src: the pack reader is engine-internal, and list/verify read through it.
	includedirs {
		"%{wks.location}/include",
		"%{wks.location}/src",
		"%{IncludeDir.glm}",
		"%{IncludeDir.spdlog}"
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
		optimize "On"
		symbols "Off"
		defines { "DE_DISTRIBUTION" }

	copyAssimpRuntime()
