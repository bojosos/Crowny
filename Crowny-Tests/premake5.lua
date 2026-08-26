project "Crowny-Tests"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	staticruntime "off"

	targetdir ("%{wks.location}/bin/" .. engineoutputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. engineoutputdir .. "/%{prj.name}")
	debugdir ("%{wks.location}/Crowny-Editor")

	applySanitizer(true)

	files
	{
		"Source/**.h",
		"Source/**.cpp",
		"%{wks.location}/Crowny-Editor/Source/Build/BuildManager.cpp",
		"%{wks.location}/Crowny-Editor/Source/Editor/AssetPreviewService.cpp",
		"%{wks.location}/Crowny-Editor/Source/Editor/ImportScheduler.cpp",
		"%{wks.location}/Crowny-Editor/Source/Editor/BoxCollider2DBoundsTransaction.cpp",
		"%{wks.location}/Crowny-Editor/Source/Editor/ScriptInspectorTransaction.cpp",
		"%{wks.location}/Crowny-Editor/Source/Editor/UndoRedo.cpp",
		"%{wks.location}/Crowny-Editor/Source/Panels/ConsoleViewModel.cpp",
		"%{wks.location}/Crowny-Editor/Source/Panels/AssetBrowserOperations.cpp",
		"%{wks.location}/Crowny-Editor/Source/Panels/AssetBrowserSelection.cpp",
		"%{wks.location}/Crowny-Editor/Source/Panels/ComponentMenuModel.cpp",
		"%{wks.location}/Crowny-Editor/Source/Panels/MaterialInspectorSchemaCache.cpp",
		"%{wks.location}/Crowny-Editor/Source/Panels/ViewportHudText.cpp",
		"%{wks.location}/Crowny-Editor/Source/UI/Properties.cpp",
		"%{wks.location}/Crowny-Editor/Source/UI/PopupLabelId.cpp",
	}

	includedirs
	{
		"Source",
		"%{wks.location}/Crowny/Source",
		"%{wks.location}/Crowny-Editor/Source",
		"%{IncludeDir.catch2}",
		"%{IncludeDir.glm}",
        "%{IncludeDir.vulkan}",
		"%{IncludeDir.vulkanvma}",
		"%{IncludeDir.spdlog}",
		"%{IncludeDir.mono}",
		"%{IncludeDir.imgui}",
		"%{IncludeDir.ImGuizmo}",
		"%{IncludeDir.stb_image}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.cereal}",
		"%{wks.location}/Crowny/Dependencies/rapidjson/include",
		"%{IncludeDir.yamlcpp}",
		"%{IncludeDir.tracy}",
		"%{IncludeDir.FastNoiseLite}",
		"%{wks.location}/Crowny/Dependencies/openal-soft/include",
		"%{wks.location}/Crowny/Dependencies/vorbis/include",
		"%{wks.location}/Crowny/Dependencies/libogg/include"
	}

	links
	{
		"Crowny",
		"catch2"
	}

	linkCrownyFinalDependencies()
	deployCrownyRuntimeDependencies()

	dependson
	{
		"Crowny"
	}

	filter "system:windows"
		systemversion "latest"
		buildoptions { "/utf-8" }

		defines
		{
			"CW",
			"CW_WINDOWS",
			"CW_PLATFORM_WIN32",
		}

	filter "configurations:Debug"
		defines { "CW_DEBUG" }
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines "CW_RELEASE"
		runtime "Release"
		optimize "on"
