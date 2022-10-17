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
IncludeDir["Box2D"] = "%{wks.location}/Crowny/Dependencies/box2d/include"
IncludeDir["imgui"] = "%{wks.location}/Crowny/Dependencies/imgui"
IncludeDir["glm"] = "%{wks.location}/Crowny/Dependencies/glm"
IncludeDir["entt"] = "%{wks.location}/Crowny/Dependencies/entt/single_include"
IncludeDir["stb_image"] = "%{wks.location}/Crowny/Dependencies/stb_image"
IncludeDir["assimp"] = "%{wks.location}/Crowny/Dependencies/assimp/include"
IncludeDir["cereal"] = "%{wks.location}/Crowny/Dependencies/cereal/include"
IncludeDir['yamlcpp'] = "%{wks.location}/Crowny/Dependencies/yaml-cpp/include"
IncludeDir["ImGuizmo"] = "%{wks.location}/Crowny/Dependencies/ImGuizmo"
IncludeDir["openal"] = "%{wks.location}/Crowny/Dependencies/openal-soft/include" -- this one is also somewhat installable
IncludeDir["libvorbis"] = "%{wks.location}/Crowny/Dependencies/vorbis/include"
IncludeDir["libogg"] = "%{wks.location}/Crowny/Dependencies/libogg/include"

-- installed/platform
filter "system:linux"
	IncludeDir["gtk"] = "/usr/include/gtk-3.0/" --- fix for windows
	IncludeDir["glib"] = "/usr/include/glib-2.0"
	IncludeDir['vulkan'] = "/usr/include/vulkan"
	IncludeDir["mono"] = "/usr/include/mono-2.0"
filter "system:windows"
	IncludeDir["mono"] = "C:/Program Files/Mono/include/mono-2.0"
	IncludeDir['vulkan'] = "C:/VulkanSDK/1.3.224.1/Include"
	
group "Dependencies"
	include "3rdparty/premake"
	include "Crowny/Dependencies/glfw"
	include "Crowny/Dependencies/glad"
	include "Crowny/Dependencies/freetype-gl"
	include "Crowny/Dependencies/freetype2"
	include "Crowny/Dependencies/imgui"
	include "Crowny/Dependencies/assimp"
  	include "Crowny/Dependencies/yaml-cpp"
	include "Crowny/Dependencies/ImGuizmo"
	include "Crowny/Dependencies/box2d"
	include "Crowny/Dependencies/vorbis"
	include "Crowny/Dependencies/libogg"
group ""

include "Crowny"
include "Crowny-Editor"
include "Crowny-Sandbox"
include "Crowny-Sharp"
include "Crowny-Sandbox"
