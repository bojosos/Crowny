project "Crowny-Builder"
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
        "%{wks.location}/Crowny/Source",
        "%{wks.location}/Crowny/Dependencies/spdlog/include",
        "%{wks.location}/Crowny/Dependencies/rapidjson/include",
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
        "%{IncludeDir.tracy}",
        "%{IncludeDir.FastNoiseLite}",
        "%{wks.location}/Crowny/Dependencies/openal-soft/include",
        "%{wks.location}/Crowny/Dependencies/vorbis/include",
        "%{wks.location}/Crowny/Dependencies/libogg/include",
    }

    links { "Crowny" }
    linkCrownyFinalDependencies()
    deployCrownyRuntimeDependencies()

    dependson { "Crowny" }

    filter "system:windows"
        systemversion "latest"
        buildoptions { "/utf-8" }
        defines { "CW", "CW_WINDOWS", "CW_PLATFORM_WIN32" }

    filter "system:linux"
        systemversion "latest"
        defines { "CW", "CW_PLATFORM_LINUX" }

    filter "configurations:Debug"
        defines { "CW_DEBUG" }
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        defines "CW_RELEASE"
        runtime "Release"
        optimize "on"

    filter "configurations:Dist"
        defines "CW_DIST"
        runtime "Release"
        optimize "on"

    filter {}
