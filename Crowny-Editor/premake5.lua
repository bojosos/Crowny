project "Crowny-Editor"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "on"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "cwepch.h"
	pchsource "Source/cwepch.cpp"

	files
	{
		"Source/**.h",
		"Source/**.cpp"
	}

  toolset "clang"

	includedirs
	{
		"%{wks.location}/Crowny/Dependencies/spdlog/include",
		"%{wks.location}/Crowny/Source",
		"Source",
		"Crowny-Editor/Source",
		"%{IncludeDir.imgui}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.assimp}",
		"%{IncludeDir.mono}",
   	"%{IncludeDir.vulkan}",
		"%{IncludeDir.ImGuizmo}",
    "%{IncludeDir.openal}"
	}

	libdirs
	{
		"/usr/lib/mono-2.0",
		"%{wks.location}/Crowny/Dependencies/vulkan/lib",
		"%{wks.location}/Crowny/Dependencies/openal-soft/build"
	}

	links 
	{
		"ImGuizmo",
		"Crowny",
		"GL", "Xxf86vm", "Xrandr", "pthread", "Xi", "dl", "uuid",
		"imgui",
		"freetype-gl",
		"assimp",
		"freetype2", "glfw", "glad",
		"mono-2.0",
		"yaml-cpp",
		"vulkan",
		"shaderc_shared",
		"spirv-cross-core",
		"openal",
    "ogg",
    "vorbis",
    "vorbisfile"
	}

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
			"CW_PLATFORM_LINUX",
			"GLFW_INCLUDE_NONE"
		}

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
