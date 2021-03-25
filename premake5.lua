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
IncludeDir["gtk"] = "/usr/include/gtk-3.0/" --- fix for windows
IncludeDir["glib"] = "/usr/include//glib-2.0"
IncludeDir["Bitsery"] = "%{wks.location}/Crowny/Dependencies/bitsery/include/bitsery"
--"/usr/include/gtk-3.0 -I/usr/include/at-spi2-atk/2.0 -I/usr/include/at-spi-2.0 -I/usr/include/dbus-1.0 -I/usr/lib/x86_64-linux-gnu/dbus-1.0/include -I/usr/include/gtk-3.0 -I/usr/include/gio-unix-2.0 -I/usr/include/cairo -I/usr/include/pango-1.0 -I/usr/include/fribidi -I/usr/include/harfbuzz -I/usr/include/atk-1.0 -I/usr/include/cairo -I/usr/include/pixman-1 -I/usr/include/uuid -I/usr/include/freetype2 -I/usr/include/libpng16 -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/libmount -I/usr/include/blkid -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include

IncludeDir['vulkan'] = "%{wks.location}/Crowny/Dependencies/vulkan/include"
IncludeDir['yamlcpp'] = "%{wks.location}/Crowny/Dependencies/yaml-cpp/include"
IncludeDir["ImGuizmo"] = "%{wks.location}/Crowny/Dependencies/ImGuizmo"

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
group ""

include "Crowny"
include "Crowny-Editor"
include "Crowny-Sandbox"
include "Crowny-Sharp"
include "Crowny-Sandbox"
