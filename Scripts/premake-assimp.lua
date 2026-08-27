project "assimp"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "off"

    local assimpRoot = "%{wks.location}/Crowny/Dependencies/assimp"
    targetdir (assimpRoot .. "/bin/" .. outputdir .. "/%{prj.name}")
    objdir (assimpRoot .. "/bin-int/" .. outputdir .. "/%{prj.name}")

    defines {
        "ASSIMP_BUILD_NO_OWN_ZLIB",

        "_CRT_SECURE_NO_WARNINGS",
        -- Crowny also links a newer RapidJSON release. Keep Assimp's bundled
        -- templates in a distinct namespace so COMDAT folding cannot mix the
        -- two incompatible class layouts in the final executable.
        "RAPIDJSON_NAMESPACE=AssimpRapidJson",
        "rapidjson=AssimpRapidJson",
        "RAPIDJSON_NOMEMBERITERATORCLASS",
        "RAPIDJSON_HAS_STDSTRING",

        "ASSIMP_BUILD_NO_X_IMPORTER",
        "ASSIMP_BUILD_NO_3DS_IMPORTER",
        "ASSIMP_BUILD_NO_MD3_IMPORTER",
        "ASSIMP_BUILD_NO_MDL_IMPORTER",
        "ASSIMP_BUILD_NO_MD2_IMPORTER",
        "ASSIMP_BUILD_NO_ASE_IMPORTER",
        "ASSIMP_BUILD_NO_AMF_IMPORTER",
        "ASSIMP_BUILD_NO_HMP_IMPORTER",
        "ASSIMP_BUILD_NO_SMD_IMPORTER",
        "ASSIMP_BUILD_NO_MDC_IMPORTER",
        "ASSIMP_BUILD_NO_MD5_IMPORTER",
        "ASSIMP_BUILD_NO_STL_IMPORTER",
        "ASSIMP_BUILD_NO_LWO_IMPORTER",
        "ASSIMP_BUILD_NO_DXF_IMPORTER",
        "ASSIMP_BUILD_NO_NFF_IMPORTER",
        "ASSIMP_BUILD_NO_RAW_IMPORTER",
        "ASSIMP_BUILD_NO_OFF_IMPORTER",
        "ASSIMP_BUILD_NO_AC_IMPORTER",
        "ASSIMP_BUILD_NO_BVH_IMPORTER",
        "ASSIMP_BUILD_NO_IRRMESH_IMPORTER",
        "ASSIMP_BUILD_NO_IRR_IMPORTER",
        "ASSIMP_BUILD_NO_Q3D_IMPORTER",
        "ASSIMP_BUILD_NO_B3D_IMPORTER",
        "ASSIMP_BUILD_NO_TERRAGEN_IMPORTER",
        "ASSIMP_BUILD_NO_CSM_IMPORTER",
        "ASSIMP_BUILD_NO_3D_IMPORTER",
        "ASSIMP_BUILD_NO_LWS_IMPORTER",
        "ASSIMP_BUILD_NO_OGRE_IMPORTER",
        "ASSIMP_BUILD_NO_OPENGEX_IMPORTER",
        "ASSIMP_BUILD_NO_MS3D_IMPORTER",
        "ASSIMP_BUILD_NO_COB_IMPORTER",
        "ASSIMP_BUILD_NO_BLEND_IMPORTER",
        "ASSIMP_BUILD_NO_Q3BSP_IMPORTER",
        "ASSIMP_BUILD_NO_NDO_IMPORTER",
        "ASSIMP_BUILD_NO_IFC_IMPORTER",
        "ASSIMP_BUILD_NO_XGL_IMPORTER",
        "ASSIMP_BUILD_NO_ASSBIN_IMPORTER",
        "ASSIMP_BUILD_NO_C4D_IMPORTER",
        "ASSIMP_BUILD_NO_3MF_IMPORTER",
        "ASSIMP_BUILD_NO_X3D_IMPORTER",
        "ASSIMP_BUILD_NO_MMD_IMPORTER",

        "ASSIMP_BUILD_NO_STEP_EXPORTER",
        "ASSIMP_BUILD_NO_SIB_IMPORTER",

        "ASSIMP_BUILD_NO_GENFACENORMALS_PROCESS",
        "ASSIMP_BUILD_NO_REMOVEVC_PROCESS",
        "ASSIMP_BUILD_NO_SPLITLARGEMESHES_PROCESS",
        "ASSIMP_BUILD_NO_PRETRANSFORMVERTICES_PROCESS",
        "ASSIMP_BUILD_NO_LIMITBONEWEIGHTS_PROCESS",
        "ASSIMP_BUILD_NO_IMPROVECACHELOCALITY_PROCESS",
        "ASSIMP_BUILD_NO_FIXINFACINGNORMALS_PROCESS",
        "ASSIMP_BUILD_NO_REMOVE_REDUNDANTMATERIALS_PROCESS",
        "ASSIMP_BUILD_NO_FINDINVALIDDATA_PROCESS",
        "ASSIMP_BUILD_NO_FINDDEGENERATES_PROCESS",
        "ASSIMP_BUILD_NO_SORTBYPTYPE_PROCESS",
        "ASSIMP_BUILD_NO_GENUVCOORDS_PROCESS",
        "ASSIMP_BUILD_NO_TRANSFORMTEXCOORDS_PROCESS",
        "ASSIMP_BUILD_NO_FINDINSTANCES_PROCESS",
        "ASSIMP_BUILD_NO_OPTIMIZEMESHES_PROCESS",
        "ASSIMP_BUILD_NO_OPTIMIZEGRAPH_PROCESS",
        "ASSIMP_BUILD_NO_SPLITBYBONECOUNT_PROCESS",
        "ASSIMP_BUILD_NO_DEBONE_PROCESS",
        "ASSIMP_BUILD_NO_EMBEDTEXTURES_PROCESS",
        "ASSIMP_BUILD_NO_GLOBALSCALE_PROCESS",
    }

    files {
        assimpRoot .. "/include/**",
        assimpRoot .. "/code/Common/**",
        assimpRoot .. "/code/CApi/**",
        assimpRoot .. "/code/AssetLib/FBX/**",
        assimpRoot .. "/code/AssetLib/M3D/**",
        assimpRoot .. "/code/AssetLib/IQM/**",
        assimpRoot .. "/code/AssetLib/glTF/**",
        assimpRoot .. "/code/AssetLib/glTF2/**",
        assimpRoot .. "/code/AssetLib/Collada/**",
        assimpRoot .. "/code/AssetLib/Obj/**",
        assimpRoot .. "/code/AssetLib/Ply/**",

        -- Common/Exporter.cpp registers these formats even when their importers are disabled.
        assimpRoot .. "/code/AssetLib/3DS/3DSExporter.cpp",
        assimpRoot .. "/code/AssetLib/3DS/3DSExporter.h",
        assimpRoot .. "/code/AssetLib/Assbin/AssbinExporter.cpp",
        assimpRoot .. "/code/AssetLib/Assbin/AssbinExporter.h",
        assimpRoot .. "/code/AssetLib/Assbin/AssbinFileWriter.cpp",
        assimpRoot .. "/code/AssetLib/Assbin/AssbinFileWriter.h",
        assimpRoot .. "/code/AssetLib/Assjson/cencode.c",
        assimpRoot .. "/code/AssetLib/Assjson/cencode.h",
        assimpRoot .. "/code/AssetLib/Assjson/json_exporter.cpp",
        assimpRoot .. "/code/AssetLib/Assjson/mesh_splitter.cpp",
        assimpRoot .. "/code/AssetLib/Assjson/mesh_splitter.h",
        assimpRoot .. "/code/AssetLib/Assxml/AssxmlExporter.cpp",
        assimpRoot .. "/code/AssetLib/Assxml/AssxmlExporter.h",
        assimpRoot .. "/code/AssetLib/Assxml/AssxmlFileWriter.cpp",
        assimpRoot .. "/code/AssetLib/Assxml/AssxmlFileWriter.h",
        assimpRoot .. "/code/AssetLib/STL/STLExporter.cpp",
        assimpRoot .. "/code/AssetLib/STL/STLExporter.h",
        assimpRoot .. "/code/AssetLib/X/XFileExporter.cpp",
        assimpRoot .. "/code/AssetLib/X/XFileExporter.h",
        assimpRoot .. "/code/AssetLib/X3D/X3DExporter.cpp",
        assimpRoot .. "/code/AssetLib/X3D/X3DExporter.hpp",
        assimpRoot .. "/code/AssetLib/3MF/D3MFExporter.cpp",
        assimpRoot .. "/code/AssetLib/3MF/D3MFExporter.h",
        assimpRoot .. "/code/Pbrt/PbrtExporter.cpp",
        assimpRoot .. "/code/Pbrt/PbrtExporter.h",

        assimpRoot .. "/code/PostProcessing/**",
        assimpRoot .. "/code/Material/**",
        assimpRoot .. "/code/Geometry/**",
        assimpRoot .. "/contrib/irrXML/**",
        assimpRoot .. "/contrib/pugixml/**",
        assimpRoot .. "/contrib/zlib/*.c",
        assimpRoot .. "/contrib/zlib/*.h",
        assimpRoot .. "/contrib/utf8cpp/source/**",
        assimpRoot .. "/contrib/unzip/**",
        assimpRoot .. "/contrib/zip/src/miniz.h",
        assimpRoot .. "/contrib/zip/src/zip.c",
        assimpRoot .. "/contrib/zip/src/zip.h",
    }

    includedirs {
        assimpRoot,
        assimpRoot .. "/include",
        assimpRoot .. "/code",
        assimpRoot .. "/contrib/irrXML",
        assimpRoot .. "/contrib/zlib",
        assimpRoot .. "/contrib/rapidjson/include",
        assimpRoot .. "/contrib/pugixml/src",
        assimpRoot .. "/contrib/utf8cpp/source",
        "%{wks.location}/Crowny/Dependencies/stb_image",
        assimpRoot .. "/contrib/unzip",
        "%{wks.location}/Scripts",
    }

    forceincludes { "%{wks.location}/Scripts/AssimpForceInclude.h" }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

    filter {}
