project "Crowny-RenderTests"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    targetdir ("%{wks.location}/bin/" .. engineoutputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-int/" .. engineoutputdir .. "/%{prj.name}")
    debugdir ("%{wks.location}")

    applySanitizer(true)

    files
    {
        "Source/**.h",
        "Source/**.cpp",
    }

    includedirs
    {
        "Source",
        "%{wks.location}/Crowny/Source",
        "%{IncludeDir.glm}",
        "%{IncludeDir.vulkan}",
        "%{IncludeDir.vulkanvma}",
        "%{IncludeDir.spdlog}",
        "%{IncludeDir.mono}",
        "%{IncludeDir.imgui}",
        "%{IncludeDir.stb_image}",
        "%{IncludeDir.entt}",
        "%{IncludeDir.cereal}",
        "%{IncludeDir.yamlcpp}",
        "%{IncludeDir.FastNoiseLite}",
        "%{wks.location}/Crowny/Dependencies/openal-soft/include",
        "%{wks.location}/Crowny/Dependencies/vorbis/include",
        "%{wks.location}/Crowny/Dependencies/libogg/include"
    }

    links { "Crowny" }
    linkCrownyFinalDependencies()
    deployCrownyRuntimeDependencies()

    dependson { "Crowny" }

    filter "system:windows"
        systemversion "latest"
        buildoptions { "/utf-8" }
        defines { "CW", "CW_WINDOWS", "CW_PLATFORM_WIN32" }

    filter "configurations:Debug or DebugASan"
        defines { "CW_DEBUG" }
        runtime "Debug"
        symbols "on"

    filter "configurations:Release or ReleaseASan"
        defines { "CW_RELEASE" }
        runtime "Release"
        optimize "on"

    filter "configurations:Dist"
        defines { "CW_DIST" }
        runtime "Release"
        optimize "on"

    filter {}
