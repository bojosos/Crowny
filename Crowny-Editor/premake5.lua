project "Crowny-Editor"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "cwepch.h"
	pchsource "Source/cwepch.cpp"

	files
	{
		"Source/**.h",
		"Source/**.cpp"
	}

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
    	"%{IncludeDir.openal}",
		"%{IncludeDir.cereal}",
		"%{IncludeDir.yamlcpp}",
		"%{IncludeDir.libvorbis}",
		"%{IncludeDir.libogg}",
		"%{IncludeDir.Box2D}",
		"%{IncludeDir.vkalloc}"
	}

	links 
	{
		"assimp",
		"Box2D",
		"imgui",
		"ImGuizmo",

		"freetype-gl",
		"freetype2",

		"glfw",
		"glad",
		
		"yaml-cpp",
		
		"Crowny"
	}

	filter "system:windows"
		systemversion "latest"

		defines
		{
			"CW",
			"CW_WINDOWS",
			"GLFW_INCLUDE_NONE"
		}

		libdirs
		{
			"%{wks.location}/Crowny/Dependencies/vorbis/bin/Debug-windows-x86_64/libvorbis",
			"C:/Program Files/Mono/lib",
			"C:/VulkanSDK/1.3.204.1/Lib",
			"C:/Program Files (x86)/OpenAL 1.1 SDK/libs/Win64",
			"C:/dev/Crowny/Crowny/Dependencies/libogg/bin/Debug-windows-x86_64/libogg"
		}

		links
		{
			"OpenAL32.lib",

			"vorbisenc.lib",
			"libvorbisfile_static.lib",
			"libvorbis.lib",
			"libvorbis_static.lib",
			"libogg.lib",

			"mono-2.0-sgen.lib",

			"vulkan-1.lib",
			"shaderc_sharedd.lib",
			"spirv-cross-cored.lib",

			"Rpcrt4.lib"
		}

	filter "system:linux"
		systemversion "latest"

		links
		{
			"GL",
			"Xxf86vm",
			"Xrandr",
			"pthread",
			"Xi",
			"dl",
			"uuid",
			"vulkan",
			"mono-2.0",
		}

		defines
		{
			"CW",
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
