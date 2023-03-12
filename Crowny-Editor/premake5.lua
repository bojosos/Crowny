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
		"%{wks.location}/Crowny/Dependencies/rapidjson/include",
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
		"%{IncludeDir.Box2D}"
	}

	links 
	{
		"assimp",
		"Box2D",
		"imgui",
		"ImGuizmo",

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
			"C:/Program Files/Mono/lib",
			"C:/VulkanSDK/1.3.224.1/Lib",
			"C:/Program Files (x86)/OpenAL 1.1 SDK/libs/Win64"
		}

		links
		{
			"OpenAL32.lib",

			"libvorbis",
			"libogg",

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
