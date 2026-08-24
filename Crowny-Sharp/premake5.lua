project "Crowny-Sharp"
	kind "SharedLib"
	language "C#"
	targetname "CrownySharp"
	dotnetframework "4.7.2"
	clr "Unsafe"

	targetdir ("%{wks.location}/Crowny-Sharp")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"Source/**.cs",
		"../Crowny/Dependencies/FastNoiseLite/CSharp/FastNoiseLite.cs"
	}

	links
	{
		"System",
		"System.Core"
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
