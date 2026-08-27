project "Crowny"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	staticruntime "off"
	characterset ("MBCS")

	targetdir ("%{wks.location}/bin/" .. engineoutputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. engineoutputdir .. "/%{prj.name}")

	applySanitizer(true)

	pchheader "cwpch.h"
	pchsource "Source/cwpch.cpp"

	files
	{
		"Source/**.h",
		"Source/**.cpp",
		"Dependencies/stb_image/**.h",
		"Dependencies/stb_image/**.cpp",
		"Dependencies/glm/glm/**.hpp",
		"Dependencies/glm/glm/**.inl",
		"Dependencies/FastNoiseLite/Cpp/FastNoiseLite.h",
		"Dependencies/cereal/include/cereal/**.h"
	}

	defines
	{
	    "CW",
		"_CRT_NONSTDC_NO_DEPRECATE",
		"_CRT_SECURE_NO_WARNINGS"
	}

	filter "platforms:not Web"
		if PhysicsBox3D then
			defines { "CW_PHYSICS_BOX3D" }
		end
		if PhysicsJolt then
			defines { "CW_PHYSICS_JOLT" }
		end
		if PhysicsBullet then
			defines { "CW_PHYSICS_BULLET" }
		end
	filter {}

	includedirs
	{
		"Source",
		"%{wks.location}/Crowny/Source",
		"Dependencies/spdlog/include",
		"Dependencies/rapidjson/include",
		"%{IncludeDir.glfw}",
		"%{IncludeDir.glad}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.imgui}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.stb_image}",
		"%{IncludeDir.assimp}",
		"%{IncludeDir.mono}",
		"%{IncludeDir.vulkan}",
		"%{IncludeDir.yamlcpp}",
		"%{IncludeDir.ImGuizmo}",
		"%{IncludeDir.openal}",
		"%{IncludeDir.cereal}",
		"%{IncludeDir.libvorbis}",
		"%{IncludeDir.libogg}",
		"%{IncludeDir.Box2D}",
		"%{IncludeDir.msdfgen}",
		"%{IncludeDir.msdfatlasgen}",
		"%{IncludeDir.mbedtls}",
		"%{IncludeDir.vulkanvma}",
		"%{IncludeDir.tracy}",
		"%{IncludeDir.basis_universal}",
		"%{IncludeDir.meshoptimizer}",
		"Dependencies/FastNoiseLite/Cpp",
	}

	dependson(CrownyProjectDependencies)

	filter "system:windows"
		systemversion "latest"
		buildoptions { "/utf-8" }

		defines
		{
			"CW",
			"CW_WINDOWS",
			"GLFW_INCLUDE_NONE",
			"CW_PLATFORM_WIN32",
		}

	filter "action:vs*"
        buildoptions { "/bigobj" }    -- gta3.std.data is a monster

	filter { "platforms:Linux64"}
		defines
		{
			"CW_PLATFORM_LINUX",
		}
		includedirs
		{
			"%{IncludeDir.spirv}",
		}

	filter { "platforms:MacOS64"}
		defines
		{
			"CW_MACOSX"
		}

	filter { "platforms:Web" }
		defines
		{
			"CW_EMSCRIPTEN",
			"GLFW_INCLUDE_ES31"
		}

		linkoptions { "-s MAX_WEBGL_VERSION=2", "-s USE_GLFW=3", "-s TOTAL_MEMORY=512MB", "-s SAFE_HEAP=1" }

	filter "system:linux"
		systemversion "latest"
		defines
		{
			"CW_PLATFORM_LINUX",
			"GLFW_INCLUDE_NONE"
		}

	filter "configurations:Debug or DebugASan"
		includedirs
		{
			path.join(PhysicsRoot, "Debug/include"),
			path.join(PhysicsRoot, "Debug/include/bullet")
		}
		libdirs { path.join(PhysicsRoot, "Debug/lib") }
		defines { "CW_DEBUG" }
		runtime "Debug"
		symbols "on"
	filter "configurations:Release or ReleaseASan"
		includedirs
		{
			path.join(PhysicsRoot, "Release/include"),
			path.join(PhysicsRoot, "Release/include/bullet")
		}
		libdirs { path.join(PhysicsRoot, "Release/lib") }
		defines "CW_RELEASE"
		runtime "Release"
		optimize "on"
	filter "configurations:Dist"
		includedirs
		{
			path.join(PhysicsRoot, "Release/include"),
			path.join(PhysicsRoot, "Release/include/bullet")
		}
		libdirs { path.join(PhysicsRoot, "Release/lib") }
		defines "CW_DIST"
		runtime "Release"
		optimize "on"
