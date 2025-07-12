project "DingoEngine"
    kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	staticruntime "off"

    targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
	objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "depch.h"
	pchsource "src/depch.cpp"

	files {
		"include/**.h",
		"include/**.hpp",
		"include/**.inl",
		"src/**.h",
		"src/**.c",
		"src/**.hpp",
		"src/**.cpp"
	}
	
	includedirs { 
		"include", 
		"src",
        "%{IncludeDir.spdlog}",
        "%{IncludeDir.glfw}",
        "%{IncludeDir.vulkan}",
        "%{IncludeDir.nvrhi}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.stb}",
		"%{IncludeDir.imgui}"
	}

	links {
		"spdlog",
		"glfw",
		"%{Library.vulkan}",
		"nvrhi",
		"imgui"
	}

	defines {
		"GLFW_INCLUDE_NONE"
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