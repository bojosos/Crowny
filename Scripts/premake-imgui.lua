project "imgui"
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "Off"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	local imguiRoot = "%{wks.location}/Crowny/Dependencies/imgui"
	files
	{
		imguiRoot .. "/imgui.cpp",
		imguiRoot .. "/imgui.h",
		imguiRoot .. "/imgui_demo.cpp",
		imguiRoot .. "/imgui_draw.cpp",
		imguiRoot .. "/imgui_internal.h",
		imguiRoot .. "/imgui_stacklayout.cpp",
		imguiRoot .. "/imgui_stacklayout.h",
		imguiRoot .. "/imgui_stacklayout_internal.h",
		imguiRoot .. "/imgui_tables.cpp",
		imguiRoot .. "/imgui_widgets.cpp",
		imguiRoot .. "/imconfig.h",
		imguiRoot .. "/imstb_rectpack.h",
		imguiRoot .. "/imstb_textedit.h",
		imguiRoot .. "/imstb_truetype.h",
		imguiRoot .. "/misc/cpp/imgui_stdlib.cpp",
		imguiRoot .. "/misc/cpp/imgui_stdlib.h"
	}

	includedirs { imguiRoot }

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
