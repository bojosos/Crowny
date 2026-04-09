include "./3rdparty/premake/premake_customization/solution_items.lua"

newoption {
	trigger = "simd",
	value = "LEVEL",
	description = "SIMD instruction set for Linux builds (default: avx2)",
	default = "avx2",
	allowed = {
		{ "sse4.1", "SSE 4.1 (CI / older CPUs)" },
		{ "avx2", "AVX2 (modern desktop CPUs)" }
	}
}

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

editandcontinue "Off"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

IncludeDir = {}
IncludeDir["glfw"] = "%{wks.location}/Crowny/Dependencies/glfw/include"
IncludeDir["glad"] = "%{wks.location}/Crowny/Dependencies/glad/include"
IncludeDir["Box2D"] = "%{wks.location}/Crowny/Dependencies/box2d/include"
IncludeDir["imgui"] = "%{wks.location}/Crowny/Dependencies/imgui"
IncludeDir["glm"] = "%{wks.location}/Crowny/Dependencies/glm"
IncludeDir["entt"] = "%{wks.location}/Crowny/Dependencies/entt/single_include"
IncludeDir["stb_image"] = "%{wks.location}/Crowny/Dependencies/stb_image"
IncludeDir["assimp"] = "%{wks.location}/Crowny/Dependencies/assimp/include"
IncludeDir["cereal"] = "%{wks.location}/Crowny/Dependencies/cereal/include"
IncludeDir["spdlog"] = "%{wks.location}/Crowny/Dependencies/spdlog/include"
IncludeDir['yamlcpp'] = "%{wks.location}/Crowny/Dependencies/yaml-cpp/include"
IncludeDir["ImGuizmo"] = "%{wks.location}/Crowny/Dependencies/ImGuizmo"
IncludeDir["openal"] = "%{wks.location}/Crowny/Dependencies/openal-soft/include" -- this one is also somewhat installable
IncludeDir["libvorbis"] = "%{wks.location}/Crowny/Dependencies/vorbis/include"
IncludeDir["libogg"] = "%{wks.location}/Crowny/Dependencies/libogg/include"
IncludeDir["msdfgen"] = "%{wks.location}/Crowny/Dependencies/msdf-atlas-gen/msdfgen"
IncludeDir["msdfatlasgen"] = "%{wks.location}/Crowny/Dependencies/msdf-atlas-gen/msdf-atlas-gen"
IncludeDir["mbedtls"] = "%{wks.location}/Crowny/Dependencies/mbedtls/include"
IncludeDir["tracy"] = "%{wks.location}/Crowny/Dependencies/tracy/public"
IncludeDir["basis_universal"] = "%{wks.location}/Crowny/Dependencies/"
IncludeDir["catch2"] = "%{wks.location}/Crowny/Dependencies/catch2/src"

-- installed/platform
if os.host() == "linux" then
	local function pkg_config(lib)
		local ok, output = pcall(os.outputof, "pkg-config --cflags-only-I " .. lib)
		if ok and output then
			return output:gsub("-I", ""):gsub("\n", " "):gsub("^%s*(.-)%s*$", "%1")
		end
		return nil
	end

	IncludeDir["gtk"] = pkg_config("gtk+-3.0") or "/usr/include/gtk-3.0/"
	IncludeDir["glib"] = pkg_config("glib-2.0") or "/usr/include/glib-2.0"
	IncludeDir["vulkan"] = "/usr/include/vulkan"
	IncludeDir["vulkanvma"] = "%{wks.location}/Crowny/Dependencies/vulkan/include"
	IncludeDir["mono"] = "/usr/include/mono-2.0"
	IncludeDir["spriv"] = "/usr/local/include"
end
if os.host() == "windows" then
	IncludeDir["mono"] = os.getenv("MONO_SDK") or "C:/Program Files/Mono/include/mono-2.0"
	
	local vulkanSDK = os.getenv("VULKAN_SDK")
	if vulkanSDK then
		IncludeDir["vulkan"] = vulkanSDK .. "/Include"
	else
		IncludeDir["vulkan"] = "C:/VulkanSDK/1.3.280.0/Include"
	end
end
	
group "Dependencies"
	include "3rdparty/premake"
	include "Crowny/Dependencies/glfw"
	include "Crowny/Dependencies/glad"
	include "Crowny/Dependencies/imgui"
	include "Crowny/Dependencies/assimp"
  	include "Crowny/Dependencies/yaml-cpp"
	include "Crowny/Dependencies/ImGuizmo"
	include "Crowny/Dependencies/box2d"
	include "Crowny/Dependencies/vorbis"
	include "Crowny/Dependencies/libogg"
	include "Crowny/Dependencies/msdf-atlas-gen"
	include "Crowny/Dependencies/mbedtls"
	include "Crowny/Dependencies/tracy"
	include "Crowny/Dependencies/basis_universal"
	include "Crowny/Dependencies/catch2"
group ""

include "Crowny"
include "Crowny-Editor"
include "Crowny-Sandbox"
include "Crowny-Sharp"
include "Crowny-Tests"
