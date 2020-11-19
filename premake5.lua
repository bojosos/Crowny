include "./3rdparty/premake/premake_customization/solution_items.lua"

workspace "Crowny"
	architecture "x86_64"
	startproject "Crowny-Editor"

	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

	platforms
	{
		"Win64",
		"Linux64",
		"MacOS64",
		"Web"
	}
	
	flags
	{
		"MultiProcessorCompile"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

IncludeDir = {}
IncludeDir["glfw"] = "%{wks.location}/Crowny/Dependencies/glfw/include"
IncludeDir["glad"] = "%{wks.location}/Crowny/Dependencies/glad/include"
IncludeDir["freetypegl"] = "%{wks.location}/Crowny/Dependencies/freetype-gl"
IncludeDir["imgui"] = "%{wks.location}/Crowny/Dependencies/imgui"
IncludeDir["glm"] = "%{wks.location}/Crowny/Dependencies/glm"
IncludeDir["entt"] = "%{wks.location}/Crowny/Dependencies/entt/single_include"
IncludeDir["stb_image"] = "%{wks.location}/Crowny/Dependencies/stb_image"
IncludeDir["assimp"] = "%{wks.location}/Crowny/Dependencies/assimp/include"
--IncludeDir["mono"] = "Crowny/Dependencies/mono/include/mono-2.0"
IncludeDir["mono"] = "/usr/include/mono-2.0" --- huh
IncludeDir['vulkan'] = "%{wks.location}/Crowny/Dependencies/vulkan/include"

group "Dependencies"
	include "3rdparty/premake"
	include "Crowny/Dependencies/glfw"
	include "Crowny/Dependencies/glad"
	include "Crowny/Dependencies/freetype-gl"
	include "Crowny/Dependencies/freetype2"
	include "Crowny/Dependencies/imgui"
	include "Crowny/Dependencies/assimp"
	include "Crowny/Dependencies/yaml-cpp"
group ""

include "Crowny"
include "Crowny-Editor"
include "Crowny-Sandbox"
include "Crowny-Sharp"
include "Crowny-Sandbox"