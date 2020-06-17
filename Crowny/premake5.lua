
project "Crowny"
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "cwpch.h"
	pchsource "Crowny/Source/cwpch.cpp"

	files
	{
		"%{prj.name}/Source/**.h",
		"%{prj.name}/Source/**.cpp",
		"%{prj.name}/Dependencies/stb_image/**.h",
		"%{prj.name}/Dependencies/freetype-gl/freetype-gl.h",
		"%{prj.name}/Dependencies/stb_image/**.cpp",
		"%{prj.name}/Dependencies/glm/glm/**.hpp",
		"%{prj.name}/Dependencies/glm/glm/**.inl",
		"%{prj.name}/Dependencies/sol/sol.hpp",
		"%{prj.name}/res/**",
		"%{prj.name}/lua/**"
	}

	defines
	{
		"_CRT_NONSTDC_NO_DEPRECATE",
		"_CRT_SECURE_NO_WARNINGS"
	}

	includedirs
	{
		"%{prj.name}/Source",
		"%{prj.name}/Dependencies/spdlog/include",
		"%{IncludeDir.glfw}",
		"%{IncludeDir.freetypegl}",
		"%{IncludeDir.glad}",
		"%{IncludeDir.imgui}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.stb_image}",
		"%{IncludeDir.lua}",
		"%{IncludeDir.sol}"
	}

	links 
	{ 
		"lua",
		"imgui",
		"freetype-gl"
	}

	filter { "platforms:Win64" }
		links { "freetyp2", "glfw", "glad", "opengl32.lib" }

		defines
		{
			"CW_WIN"
		}
		system("windows")

	filter { "platforms:Linux64"}
		links { "freetyp2", "glfw", "glad" }

		defines
		{
			"CW_LINUX"
		}

		system("linux")

	filter { "platforms:MacOS64"}
		links { "freetyp2", "glfw", "glad" }

		defines
		{
			"CW_MACOSX"
		}

		system("macosx")

	filter { "platforms:Web" }
		defines 
		{
			"CW_EMSCRIPTEN",
			"GLFW_INCLUDE_ES31"
		}

		linkoptions { "-s USE_FREETYPE=1", "-s MAX_WEBGL_VERSION=2", "-s USE_GLFW=3", "-s TOTAL_MEMORY=512MB", "-s SAFE_HEAP=1" }

	filter "system:windows"
		systemversion "latest"

		defines
		{
			"CW_WINDOWS",
			"GLFW_INCLUDE_NONE"
		}

	filter "system:linux"
		systemversion "latest"
		defines
		{
			"CW_LINUX",
			"GLFW_INCLUDE_NONE"
		}

	filter "configurations:Debug"
		defines { "CW_DEBUG", "SOL_ALL_SAFETIES_ON" }
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines "MC_RELEASE"
		runtime "Release"
		optimize "on"
