project "Crowny-Tests"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	staticruntime "off"

	targetdir ("%{wks.location}/bin/" .. engineoutputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. engineoutputdir .. "/%{prj.name}")
	debugdir ("%{wks.location}")

	applySanitizer(true)

	pchheader "cwtpch.h"
	pchsource "Source/cwtpch.cpp"
	forceincludes { "cwtpch.h" }

	files
	{
		"Source/**.h",
		"Source/**.cpp",
		"%{wks.location}/Crowny-Editor/Source/Build/BuildManager.cpp",
		"%{wks.location}/Crowny-Editor/Source/Editor/AssetPreviewService.cpp",
		"%{wks.location}/Crowny-Editor/Source/Editor/AssetLibraryServices.cpp",
		"%{wks.location}/Crowny-Editor/Source/Editor/ImportScheduler.cpp",
		"%{wks.location}/Crowny-Editor/Source/Editor/BoxCollider2DBoundsTransaction.cpp",
		"%{wks.location}/Crowny-Editor/Source/Editor/EntityFactory.cpp",
		"%{wks.location}/Crowny-Editor/Source/Editor/ScriptInspectorTransaction.cpp",
		"%{wks.location}/Crowny-Editor/Source/Editor/Script/ManagedProjectDependencies.cpp",
		"%{wks.location}/Crowny-Editor/Source/Editor/Script/ScriptProjectGenerator.cpp",
		"%{wks.location}/Crowny-Editor/Source/Editor/Script/VSCodeEditor.cpp",
		"%{wks.location}/Crowny-Editor/Source/Editor/UndoRedo.cpp",
		"%{wks.location}/Crowny-Editor/Source/Editor/ViewportTransformInteraction.cpp",
		"%{wks.location}/Crowny-Editor/Source/Panels/ConsoleViewModel.cpp",
		"%{wks.location}/Crowny-Editor/Source/Panels/AssetBrowserOperations.cpp",
		"%{wks.location}/Crowny-Editor/Source/Panels/AssetBrowserSelection.cpp",
		"%{wks.location}/Crowny-Editor/Source/Panels/ComponentMenuModel.cpp",
		"%{wks.location}/Crowny-Editor/Source/Panels/MaterialInspectorSchemaCache.cpp",
		"%{wks.location}/Crowny-Editor/Source/Panels/ScriptInspectorPath.cpp",
		"%{wks.location}/Crowny-Editor/Source/Panels/ScriptInspectorProgressBar.cpp",
		"%{wks.location}/Crowny-Editor/Source/Panels/ScriptInspectorSearch.cpp",
		"%{wks.location}/Crowny-Editor/Source/Panels/ViewportHudText.cpp",
		"%{wks.location}/Crowny-Editor/Source/Serialization/ProjectSettingsSerializer.cpp",
		"%{wks.location}/Crowny-Editor/Source/UI/Properties.cpp",
		"%{wks.location}/Crowny-Editor/Source/UI/EnumButtonsModel.cpp",
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

	filter "system:linux"
		defines
		{
			"CW",
			"CW_PLATFORM_LINUX",
		}

	filter "system:macosx"
		defines
		{
			"CW",
			"CW_MACOSX",
		}

	filter "configurations:Debug or DebugASan"
		defines { "CW_DEBUG" }
		runtime "Debug"
		symbols "on"

	filter "configurations:Release or ReleaseASan"
		defines "CW_RELEASE"
		runtime "Release"
		optimize "on"
