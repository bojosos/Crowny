include "./3rdparty/premake/premake_customization/solution_items.lua"

newoption { trigger = "without-box3d", description = "Do not compile the Box3D backend" }
newoption { trigger = "without-jolt", description = "Do not compile the Jolt backend" }
newoption { trigger = "without-bullet", description = "Do not compile the Bullet backend" }

newoption {
	trigger = "simd",
	value = "LEVEL",
	description = "Minimum SIMD instruction set for x86-64 desktop builds (default: avx2)",
	default = "avx2",
	allowed = {
		{ "sse4.1", "SSE 4.1 (CI / older CPUs)" },
		{ "avx2", "AVX2 (modern desktop CPUs)" }
	}
}

newoption {
	trigger = "with-nodes",
	description = "Enable node editor (requires ImGui 1.87+)"
}

newoption {
	trigger = "sanitizer",
	value = "TYPE",
	description = "Enable runtime sanitizer instrumentation",
	allowed = {
		{ "address", "AddressSanitizer (memory errors; leaks on supported Unix hosts)" },
		{ "thread", "ThreadSanitizer (data races)" },
		{ "undefined", "UndefinedBehaviorSanitizer" },
		{ "memory", "MemorySanitizer (uninitialized reads, clang only)" }
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

	multiprocessorcompile "On"

local simdLevel = _OPTIONS["simd"] or "avx2"
local desktopX64Filter = { "platforms:Win64 or Linux64 or MacOS64", "architecture:x86_64", "language:C or C++" }

filter(desktopX64Filter)
	if simdLevel == "avx2" then
		vectorextensions "AVX2"
		defines { "CW_SIMD_AVX2=1" }
		filter { "platforms:Win64 or Linux64 or MacOS64", "architecture:x86_64", "language:C or C++", "toolset:not msc*" }
		buildoptions { "-mbmi", "-mpopcnt", "-mlzcnt", "-mf16c" }
	elseif simdLevel == "sse4.1" then
		defines { "CW_SIMD_SSE41=1" }
		filter { "platforms:Win64 or Linux64 or MacOS64", "architecture:x86_64", "language:C or C++", "toolset:not msc*" }
		buildoptions { "-msse4.1" }
	end

filter { "platforms:Win64 or Linux64 or MacOS64", "architecture:x86_64", "language:C++" }
	defines { "GLM_FORCE_INTRINSICS" }
	if simdLevel == "avx2" then
		defines { "GLM_FORCE_AVX2", "JPH_USE_AVX2" }
	elseif simdLevel == "sse4.1" then
		defines { "GLM_FORCE_SSE41", "JPH_USE_SSE4_1" }
	end

filter {}

-- Sanitizer helper — call from project premake files
function applySanitizer(instrumentTarget)
	if not instrumentTarget or not _OPTIONS["sanitizer"] then return end
	local sanitizer = _OPTIONS["sanitizer"]
	if os.host() == "windows" and sanitizer ~= "address" then
		error("Visual Studio supports only --sanitizer=address; use clang on Linux for " .. sanitizer)
	end
	filter "toolset:msc*"
		buildoptions { "/fsanitize=address", "/Zi", "/Oy-" }
		defines { "_DISABLE_VECTOR_ANNOTATION", "_DISABLE_STRING_ANNOTATION" }
		defines { "CW_ADDRESS_SANITIZER" }
		incrementallink "Off"
		runtimechecks "Off"
		multiprocessorcompile "On"
	filter { "toolset:msc*", "configurations:Debug" }
		defines { "CW_ENABLE_CRT_LEAK_CHECKS" }
	filter { "toolset:msc*", "kind:ConsoleApp or WindowedApp", "configurations:Debug" }
		postbuildcommands {
			'{COPYFILE} "$(VCToolsInstallDir)bin\\Hostx64\\x64\\clang_rt.asan_dynamic-x86_64.dll" "%{cfg.buildtarget.directory}"',
			'{COPYFILE} "$(VCToolsInstallDir)bin\\Hostx64\\x64\\clang_rt.asan_dbg_dynamic-x86_64.dll" "%{cfg.buildtarget.directory}"'
		}
	filter { "toolset:msc*", "kind:ConsoleApp or WindowedApp", "configurations:not Debug" }
		postbuildcommands {
			'{COPYFILE} "$(VCToolsInstallDir)bin\\Hostx64\\x64\\clang_rt.asan_dynamic-x86_64.dll" "%{cfg.buildtarget.directory}"'
		}
	filter "toolset:not msc*"
		local san = "-fsanitize=" .. sanitizer
		buildoptions { san, "-fno-omit-frame-pointer" }
		linkoptions { san }
		if sanitizer == "address" then
			defines { "CW_ADDRESS_SANITIZER" }
		end
	filter {}
end

editandcontinue "Off"

filter "toolset:msc*"
	buildoptions { "/FS" }
filter {}

local sanitizerOutputSuffix = _OPTIONS["sanitizer"] and ("-" .. _OPTIONS["sanitizer"]) or ""
outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
engineoutputdir = "%{cfg.buildcfg}" .. sanitizerOutputSuffix .. "-%{cfg.system}-%{cfg.architecture}"

PhysicsRoot = os.getenv("CROWNY_PHYSICS_ROOT") or "%{wks.location}/.deps/physics/install"
VulkanRoot = os.getenv("VULKAN_SDK") or "%{wks.location}/.deps/VulkanSDK"
SpirvCrossRoot = os.getenv("CROWNY_SPIRV_CROSS_ROOT") or "%{wks.location}/.deps/spirv-cross/install"
VmaInclude = os.getenv("CROWNY_VMA_INCLUDE") or "%{wks.location}/.deps/VulkanSDK/Include"
MonoRoot = os.getenv("CROWNY_MONO_ROOT") or os.getenv("MONO_SDK") or "C:/Program Files/Mono"
local openALRoot = os.getenv("CROWNY_OPENAL_ROOT")
local openALSDK = os.getenv("OPENAL_SDK")
if openALRoot then
	OpenALLibDir = openALRoot .. "/lib"
	OpenALRuntime = openALRoot .. "/bin/OpenAL32.dll"
elseif openALSDK then
	OpenALLibDir = openALSDK .. "/libs/Win64"
	OpenALRuntime = openALSDK .. "/libs/Win64/OpenAL32.dll"
else
	OpenALLibDir = "%{wks.location}/.deps/openal/lib"
	OpenALRuntime = path.getabsolute(".deps/openal/bin/OpenAL32.dll")
end
PhysicsBox3D = not _OPTIONS["without-box3d"]
PhysicsJolt = not _OPTIONS["without-jolt"]
PhysicsBullet = not _OPTIONS["without-bullet"]

CrownyProjectDependencies =
{
	"assimp",
	"Box2D",
	"imgui",
	"ImGuizmo",
	"msdf-atlas-gen",
	"glfw",
	"glad",
	"yaml-cpp",
	"mbedtls",
	"freetype",
	"msdfgen",
	"libvorbis",
	"libogg",
	"tracy",
	"basis_universal",
	"meshoptimizer",
}

function linkCrownyFinalDependencies()
	links(CrownyProjectDependencies)

	filter "platforms:not Web"
		if PhysicsBox3D then
			links { "box3d" }
		end
		if PhysicsJolt then
			links { "Jolt" }
		end
		if PhysicsBullet then
			links { "BulletDynamics", "BulletCollision", "LinearMath" }
		end

	filter { "platforms:not Web", "system:windows" }
		libdirs
		{
			MonoRoot .. "/lib",
			VulkanRoot .. "/Lib",
			OpenALLibDir,
		}
		links
		{
			"OpenAL32.lib",
			"mono-2.0-sgen.lib",
			"vulkan-1.lib",
			"Rpcrt4.lib",
			"dbghelp.lib",
		}

	filter { "platforms:not Web", "system:linux" }
		libdirs { "/usr/local/lib" }
		links
		{
			"GL",
			"Xxf86vm",
			"Xrandr",
			"pthread",
			"Xi",
			"dl",
			"uuid",
			"vulkan",
			"mono-2.0",
			"openal",
		}

	filter { "platforms:not Web", "configurations:Debug" }
		libdirs { path.join(PhysicsRoot, "Debug/lib") }

	filter { "platforms:not Web", "configurations:Release or Dist" }
		libdirs { path.join(PhysicsRoot, "Release/lib") }

	filter { "platforms:not Web", "system:windows", "configurations:Debug" }
		libdirs { path.join(SpirvCrossRoot, "Debug/lib") }
		links
		{
			"shaderc_shared",
			path.join(SpirvCrossRoot, "Debug/lib/spirv-cross-cored.lib"),
			path.join(SpirvCrossRoot, "Debug/lib/spirv-cross-glsld.lib"),
		}

	filter { "platforms:not Web", "system:windows", "configurations:Release or Dist" }
		libdirs { path.join(SpirvCrossRoot, "Release/lib") }
		links
		{
			"shaderc_shared",
			path.join(SpirvCrossRoot, "Release/lib/spirv-cross-core.lib"),
			path.join(SpirvCrossRoot, "Release/lib/spirv-cross-glsl.lib"),
		}

	filter { "platforms:not Web", "system:not windows", "configurations:Debug" }
		links { "shaderc", "spirv-cross-core", "spirv-cross-glsl" }

	filter { "platforms:not Web", "system:not windows", "configurations:Release or Dist" }
		links { "shaderc", "spirv-cross-core", "spirv-cross-glsl" }

	filter {}
end

function deployCrownyRuntimeDependencies()
	filter "system:windows"
		postbuildcommands
		{
			'{COPYFILE} "' .. MonoRoot .. '/bin/mono-2.0-sgen.dll" "%{cfg.targetdir}/mono-2.0-sgen.dll"',
			'{COPYFILE} "' .. VulkanRoot .. '/Bin/shaderc_shared.dll" "%{cfg.targetdir}/shaderc_shared.dll"',
			'{COPYFILE} "' .. OpenALRuntime .. '" "%{cfg.targetdir}/OpenAL32.dll"',
		}
	filter {}
end

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
IncludeDir["meshoptimizer"] = "%{wks.location}/Crowny/Dependencies/meshoptimizer/src"
IncludeDir["FastNoiseLite"] = "%{wks.location}/Crowny/Dependencies/FastNoiseLite/Cpp"
IncludeDir["catch2"] = "%{wks.location}/Crowny/Dependencies/catch2/src"
if _OPTIONS["with-nodes"] then
	IncludeDir["ImNodeFlow"] = "%{wks.location}/Crowny/Dependencies/ImNodeFlow/include"
	IncludeDir["ImguiNodeEditor"] = "%{wks.location}/Crowny/Dependencies/imgui-node-editor"
end

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
	IncludeDir["vulkanvma"] = VmaInclude
	IncludeDir["mono"] = "/usr/include/mono-2.0"
	IncludeDir["spirv"] = "/usr/include"
end
if os.host() == "windows" then
	IncludeDir["mono"] = MonoRoot .. "/include/mono-2.0"
	IncludeDir["vulkanvma"] = VmaInclude

	IncludeDir["vulkan"] = VulkanRoot .. "/Include"
end

group "Dependencies"
	include "3rdparty/premake"
	include "Crowny/Dependencies/glfw"
	include "Crowny/Dependencies/glad"
	include "Scripts/premake-imgui.lua"
	include "Scripts/premake-assimp.lua"
  	include "Crowny/Dependencies/yaml-cpp"
	include "Crowny/Dependencies/ImGuizmo"
	filter {}
	project "ImGuizmo"
		defines { "IMGUI_DEFINE_MATH_OPERATORS" }
		staticruntime "Off"
	filter {}
	include "Crowny/Dependencies/box2d"
	include "Crowny/Dependencies/vorbis"
	include "Crowny/Dependencies/libogg"
	include "Crowny/Dependencies/msdf-atlas-gen"
	include "Crowny/Dependencies/mbedtls"
	include "Crowny/Dependencies/tracy"
	include "Crowny/Dependencies/basis_universal"
	include "Scripts/premake-meshoptimizer.lua"
	include "Crowny/Dependencies/catch2"
	if _OPTIONS["with-nodes"] then
		include "Crowny/Dependencies/ImNodeFlow"
		filter {}
		project "ImNodeFlow"
			defines { "IMGUI_DEFINE_MATH_OPERATORS" }
		filter {}
		include "Crowny/Dependencies/imgui-node-editor"
	end
group ""

include "Crowny"
include "Crowny-Editor"
include "Crowny-Sandbox"
include "Crowny-Sharp"
include "Crowny-Tests"
include "Crowny-RenderTests"
