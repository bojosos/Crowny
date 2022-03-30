project "Crowny-Sandbox"
	kind "ConsoleApp"
	language "C#"
	staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	links
	{
		"Crowny-Sharp"
	}

	files
	{
		"Source/**.cs"
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