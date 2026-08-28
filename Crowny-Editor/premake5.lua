project "Crowny-Editor"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	staticruntime "off"

	targetdir ("%{wks.location}/bin/" .. engineoutputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. engineoutputdir .. "/%{prj.name}")

	applySanitizer(true)

	pchheader "cwepch.h"
	pchsource "Source/cwepch.cpp"

	files
	{
		"Source/**.h",
		"Source/**.cpp"
	}

	includedirs
	{
		"%{wks.location}/Crowny/Dependencies/spdlog/include",
		"%{wks.location}/Crowny/Dependencies/rapidjson/include",
		"%{wks.location}/Crowny/Source",
		"Source",
		"Crowny-Editor/Source",
		"%{IncludeDir.imgui}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.assimp}",
	   	"%{IncludeDir.vulkan}",
		"%{IncludeDir.ImGuizmo}",
    	"%{IncludeDir.openal}",
		"%{IncludeDir.cereal}",
		"%{IncludeDir.yamlcpp}",
		"%{IncludeDir.tracy}",
		"%{IncludeDir.libvorbis}",
		"%{IncludeDir.libogg}",
		"%{IncludeDir.Box2D}"
	}

	if _OPTIONS["with-nodes"] then
		includedirs { "%{IncludeDir.ImNodeFlow}" }
		includedirs { "%{IncludeDir.ImguiNodeEditor}" }
		defines { "CW_WITH_NODES", "IMGUI_DEFINE_MATH_OPERATORS" }
	end

	links
	{
		"Crowny"
	}

	linkCrownyFinalDependencies()
	deployCrownyRuntimeDependencies()

	dependson
	{
		"Crowny"
	}

	filter "system:windows"
		prebuildcommands {
			'powershell -NoProfile -ExecutionPolicy Bypass -File "%{prj.location}/../Scripts/pack-builtins.ps1" -RepositoryRoot "%{prj.location}/.." -Configuration "%{cfg.buildcfg}"'
		}

	filter "system:linux"
		prebuildcommands {
			'python3 "%{prj.location}/../Scripts/pack-builtins.py" --repo-root "%{prj.location}/.." --configuration "%{cfg.buildcfg}"'
		}

	filter "system:macosx"
		prebuildcommands {
			'python3 "%{prj.location}/../Scripts/pack-builtins.py" --repo-root "%{prj.location}/.." --configuration "%{cfg.buildcfg}"'
		}

	filter {}

	postbuildcommands {
		'{MKDIR} "%{cfg.targetdir}/Resources"',
		'{COPYFILE} "%{prj.location}/Resources/Builtin.cwpack" "%{cfg.targetdir}/Resources/Builtin.cwpack"'
	}

	if _OPTIONS["with-nodes"] then
		links { "ImNodeFlow" }
		links { "imgui-node-editor" }
	end

	filter "system:windows"
		systemversion "latest"
		buildoptions { "/utf-8" }

		defines
		{
			"CW",
			"CW_EDITOR",
			"CW_WINDOWS",
			"GLFW_INCLUDE_NONE",
			"_CRT_SECURE_NO_WARNINGS",
			"CW_PLATFORM_WIN32",
		}

	filter "system:linux"
		systemversion "latest"

		defines
		{
			"CW",
			"CW_EDITOR",
			"CW_PLATFORM_LINUX",
			"GLFW_INCLUDE_NONE"
		}

	filter "configurations:Debug or DebugASan"
		defines "CW_DEBUG"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release or ReleaseASan"
		defines "CW_RELEASE"
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		defines "CW_DIST"
		runtime "Release"
		optimize "on"
