project "Crowny"
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"
	characterset ("MBCS")

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "cwpch.h"
	pchsource "Source/cwpch.cpp"

	files
	{
		"Source/**.h",
		"Source/**.cpp",
		"Dependencies/stb_image/**.h",
		"Dependencies/stb_image/**.cpp",
		"Dependencies/glm/glm/**.hpp",
		"Dependencies/glm/glm/**.inl",
  		"Dependencies/cereal/include/cereal/**.h"
	}

	defines
	{
	    "CW",
		"_CRT_NONSTDC_NO_DEPRECATE",
		"_CRT_SECURE_NO_WARNINGS"
	}

	includedirs
	{
		"Source",
		"Dependencies/spdlog/include",
		"Dependencies/rapidjson/include",
		"%{IncludeDir.glfw}",
		"%{IncludeDir.glad}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.imgui}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.stb_image}",
		"%{IncludeDir.assimp}",
		"%{IncludeDir.mono}",
		"%{IncludeDir.vulkan}",
		"%{IncludeDir.yamlcpp}",
		"%{IncludeDir.ImGuizmo}",
		"%{IncludeDir.openal}",
		"%{IncludeDir.cereal}",
		"%{IncludeDir.libvorbis}",
		"%{IncludeDir.libogg}",
		"%{IncludeDir.Box2D}",
		"%{IncludeDir.msdfgen}",
		"%{IncludeDir.msdfatlasgen}"
		
	}

	links 
	{
		"assimp",
		"Box2D",
		"imgui",
		"ImGuizmo",

		"msdf-atlas-gen",

		"glfw",
		"glad",
		
		"yaml-cpp",
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

	filter "action:vs*"
        buildoptions { "/bigobj" }    -- gta3.std.data is a monster

	filter { "platforms:Linux64"}

		defines
		{
			"CW_PLATFORM_LINUX",
		}

		system("linux")

	filter { "platforms:MacOS64"}
		
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

		linkoptions { "-s MAX_WEBGL_VERSION=2", "-s USE_GLFW=3", "-s TOTAL_MEMORY=512MB", "-s SAFE_HEAP=1" }

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
		defines { "CW_DEBUG" }
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines "CW_RELEASE"
		runtime "Release"
		optimize "on"
