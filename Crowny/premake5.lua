project "Crowny"
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "on"
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
    "Dependencies/bitsery/include/bitsery/**.h"
	}

--	filter { "system:windows", "files: Linux*" }
  --		flags { "ExcludeFromBuild" }

	--filter { "system:linux", "files: Windows*" }
  --		flags { "ExcludeFromBuild" }

	defines
	{
		"_CRT_NONSTDC_NO_DEPRECATE",
		"_CRT_SECURE_NO_WARNINGS"
	}

	includedirs
	{
		"Source",
		"Dependencies/spdlog/include",
		"%{IncludeDir.glfw}",
		"%{IncludeDir.freetypegl}",
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
	}

	libdirs { "/usr/lib/mono-2.0", "%{wks.location}/Crowny/Dependencies/vulkan/lib" }

	links 
	{
		"GL", "Xxf86vm", "Xrandr", "pthread", "Xi", "dl", "uuid",
		"imgui",
		"freetype-gl",
		"assimp",
		"freetype2", "glfw", "glad",
		"mono-2.0",
		"yaml-cpp",
		"ImGuizmo",
		"shaderc_shared",
    "spirv-cross-c-shared",
    "SPIRV-Tools-opt",
    "SPIRV-Tools",
    "SPIRV-Tools-link"
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
