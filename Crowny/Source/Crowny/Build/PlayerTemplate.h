#pragma once

#include "Crowny/Build/BuildTypes.h"

namespace Crowny
{
    constexpr uint32_t PLAYER_TEMPLATE_MANIFEST_SCHEMA = 1;

    struct PlayerTemplateFile
    {
        Path RelativePath;
        String Sha256;
        bool Executable = false;
    };

    struct PlayerTemplateManifest
    {
        uint32_t Schema = PLAYER_TEMPLATE_MANIFEST_SCHEMA;
        String EngineVersion;
        Vector<String> CompatibleEngineVersions;
        uint32_t PlayerAbi = 1;
        uint32_t ContentSchemaMin = 1;
        uint32_t ContentSchemaMax = 1;
        BuildPlatform Platform = BuildPlatform::WindowsX64;
        BuildConfiguration Configuration = BuildConfiguration::Development;
        Vector<RendererBackend> Renderers;
        Vector<PlayerTemplateFile> Files;
    };

    struct PlayerTemplateRequest
    {
        String EngineVersion;
        uint32_t PlayerAbi = 1;
        uint32_t ContentSchema = 1;
        BuildPlatform Platform = BuildPlatform::WindowsX64;
        BuildConfiguration Configuration = BuildConfiguration::Development;
        CompatibilityPolicy Compatibility = CompatibilityPolicy::Exact;
        Vector<RendererBackend> RequiredRenderers = { RendererBackend::Vulkan, RendererBackend::OpenGL };
    };

    class PlayerTemplateStore
    {
    public:
        static String Save(const Path& path, const PlayerTemplateManifest& manifest);
        static String Load(const Path& path, PlayerTemplateManifest& manifest);
        static String CreateManifest(const Path& root, PlayerTemplateManifest manifest, const Vector<Path>& executableFiles,
                                     PlayerTemplateManifest& output);
    };

    BuildValidation ValidatePlayerTemplate(const Path& root, const PlayerTemplateManifest& manifest, const PlayerTemplateRequest& request,
                                           BuildCancellationCheck cancellation = {});
    String StagePlayerTemplate(const Path& root, const PlayerTemplateManifest& manifest, const Path& stageDirectory,
                               BuildCancellationCheck cancellation = {});
} // namespace Crowny
