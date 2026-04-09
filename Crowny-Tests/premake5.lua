project "Crowny-Tests"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"Source/**.h",
		"Source/**.cpp",
	}

	includedirs
	{
		"Source",
		"%{wks.location}/Crowny/Source",
		"%{IncludeDir.catch2}",
		"%{IncludeDir.glm}",
        "%{IncludeDir.vulkan}",
		"%{IncludeDir.spdlog}",
		"%{IncludeDir.mono}",
		"%{IncludeDir.imgui}",
		"%{IncludeDir.stb_image}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.cereal}",
		"%{wks.location}/Crowny/Dependencies/vorbis/include",
		"%{wks.location}/Crowny/Dependencies/libogg/include"
	}

	links
	{
		"Crowny",
		"catch2"
	}

	filter "system:windows"
		systemversion "latest"
		buildoptions { "/utf-8" }

		defines
		{
			"CW",
			"CW_WINDOWS",
			"CW_PLATFORM_WIN32",
		}

		libdirs
		{
			(os.getenv("VULKAN_SDK") or "C:/VulkanSDK/1.3.280.0") .. "/Lib",
		}

		links
		{
			"vulkan-1.lib",
		}

	filter "configurations:Debug"
		defines { "CW_DEBUG" }
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines "CW_RELEASE"
		runtime "Release"
		optimize "on"
