project "meshoptimizer"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	staticruntime "Off"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	local meshoptimizerRoot = "%{wks.location}/Crowny/Dependencies/meshoptimizer"
	files
	{
		meshoptimizerRoot .. "/src/meshoptimizer.h",
		meshoptimizerRoot .. "/src/*.cpp"
	}

	includedirs { meshoptimizerRoot .. "/src" }

	filter "system:windows"
		systemversion "latest"

	filter "system:linux"
		pic "On"

	filter "configurations:Debug"
		runtime "Debug"
		symbols "On"

	filter "configurations:Release or Dist"
		runtime "Release"
		optimize "On"

	filter {}
