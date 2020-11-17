workspace "Crowny"
	architecture "x86_64"
	startproject "Crowny-Editor"

	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

	platforms
	{
		"Win64",
		"Linux64",
		"MacOS64",
		"Web"
	}
	
	flags
	{
		"MultiProcessorCompile"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

IncludeDir = {}
IncludeDir["glfw"] = "Crowny/Dependencies/glfw/include"
IncludeDir["glad"] = "Crowny/Dependencies/glad/include"
IncludeDir["freetypegl"] = "Crowny/Dependencies/freetype-gl"
IncludeDir["imgui"] = "Crowny/Dependencies/imgui"
IncludeDir["glm"] = "Crowny/Dependencies/glm"
IncludeDir["entt"] = "Crowny/Dependencies/entt/single_include"
IncludeDir["stb_image"] = "Crowny/Dependencies/stb_image"
IncludeDir["assimp"] = "Crowny/Dependencies/assimp/include"
--TODO: fix this for different platforms
--IncludeDir["mono"] = "Crowny/Dependencies/mono/include/mono-2.0"
IncludeDir["mono"] = "/usr/lib/pkgconfig/../../include/mono-2.0"
IncludeDir['vulkan'] = "Crowny/Dependencies/vulkan/include"

group "Dependencies"
	include "Crowny/Dependencies/glfw"
	include "Crowny/Dependencies/glad"
	include "Crowny/Dependencies/freetype-gl"
	include "Crowny/Dependencies/imgui"
	include "Crowny/Dependencies/freetype2"
	include "Crowny/Dependencies/assimp"
group ""

project "Crowny"
	location "Crowny"
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "on"
	characterset ("MBCS")

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "cwpch.h"
	pchsource "Crowny/Source/cwpch.cpp"

	files
	{
		"%{prj.name}/Source/**.h",
		"%{prj.name}/Source/**.cpp",
		"%{prj.name}/Dependencies/stb_image/**.h",
		"%{prj.name}/Dependencies/stb_image/**.cpp",
		"%{prj.name}/Dependencies/glm/glm/**.hpp",
		"%{prj.name}/Dependencies/glm/glm/**.inl",
		"%{prj.name}/Dependencies/entt/single_include/entt/entt.hpp",
		"%{prj.name}/Resources/**"
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
		"%{IncludeDir.entt}",
		"%{IncludeDir.imgui}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.stb_image}",
		"%{IncludeDir.assimp}",
		"%{IncludeDir.mono}",
		"%{IncludeDir.vulkan}"
	}

	libdirs { "/usr/lib/mono-2.0", "Crowny/Dependencies/vulkan/lib" }

	links 
	{
		"GL", "Xxf86vm", "Xrandr", "pthread", "Xi", "dl",
		"imgui",
		"freetype-gl",
		"assimp",
		"freetype2", "glfw", "glad",
		--y"libvulkan",
		"mono-2.0"
	}


	filter { "platforms:Win64" }
		--links { "freetype2", "glfw", "glad", "opengl32.lib" }

		defines
		{
			"CW_WIN"
		}
		system("windows")

	filter { "platforms:Linux64"}
		--links { "freetype2", "glfw", "glad" }

		defines
		{
			"CW_PLATFORM_LINUX",
			"CW_LINUX"
		}

		system("linux")

	filter { "platforms:MacOS64"}
		--links { "freetype2", "glfw", "glad" }

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
			"CW_PLATFORM_LINUX",
			"GLFW_INCLUDE_NONE"
		}

	filter "configurations:Debug"
		defines { "CW_DEBUG" }
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines "CW_RELEASE"
		runtime "Release"
		optimize "on"


project "Crowny-Editor"
	location "Crowny-Editor"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "cwepch.h"
	pchsource "Crowny-Editor/Source/cwepch.cpp"

	files
	{
		"%{prj.name}/Source/**.h",
		"%{prj.name}/Source/**.cpp"
	}

	includedirs
	{
		"Crowny/Dependencies/spdlog/include",
		"Crowny/Source",
		"Crowny-Editor/Source",
		"%{IncludeDir.imgui}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.assimp}",
		"%{IncludeDir.mono}"
	}

--	links
--	{
--		"imgui",
--		"Crowny",
--	}

	libdirs { "/usr/lib/mono-2.0", "Crowny/Dependencies/vulkan/lib" }

	links 
	{
		"GL", "Xxf86vm", "Xrandr", "pthread", "Xi", "dl",
		"Xrandr",
		"Xi",
		"GL",
		"X11",
		"Crowny",
		"imgui",
		"freetype-gl",
		"assimp",
		"mono-2.0",
		"freetype2",
		"glfw",
		"glad",
		--"libvulkan"

--		"Crowny/Dependencies/mono/lib/libmono-static-sgen.lib",
--		"Crowny/Dependencies/mono/lib/mono-2.0-sgen.lib"
	}

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		defines "CW_DEBUG"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines "CW_RELEASE"
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		defines "CW_DIST"
		runtime "Release"
		optimize "on"

project "Crowny-Sharp"
	location "Crowny-Sharp"
	kind "SharedLib"
	language "C#"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	links
	{
		"Crowny-Wrapper"
	}

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		defines "CW_DEBUG"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines "CW_RELEASE"
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		defines "CW_DIST"
		runtime "Release"
		optimize "on"

project "Crowny-Sandbox"
	location "Crowny-Sandbox"
	kind "ConsoleApp"
	language "C#"
	staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	links
	{
		"Crowny-Sharp"
	}

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		defines "CW_DEBUG"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines "CW_RELEASE"
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		defines "CW_DIST"
		runtime "Release"
		optimize "on"

