project "Crowny-Sandbox"
	kind "SharedLib"
	language "C#"
	targetname "GameAssembly"

	targetdir ("%{wks.location}/Crowny-Sandbox")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	links
	{
		"Crowny-Sharp"
	}

	dependson
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
