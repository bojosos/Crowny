#pragma once

#include "Crowny/Common/StdHeaders.h"
#include "Crowny/RenderAPI/RenderAPI.h"

namespace Crowny::RenderTests
{
    struct RunnerOptions
    {
        RenderAPI::API Backend = RenderAPI::API::Vulkan;
        Path References = "Crowny-RenderTests/References";
        Path Artifacts = "artifacts/render-tests";
        Path DecalShowcase;
        Path OslPackage;
        Path ProceduralPackage;
        Path ProceduralReference;
        Path ProceduralPreview;
        String Filter;
        bool BenchmarkSprites = false;
        bool BenchmarkSpritesSmoke = false;
        Path BenchmarkProject;
        Path BenchmarkScene;
        Path BenchmarkImport;
        Path BenchmarkTexture;
        bool BenchmarkTextureNormal = false;
        bool BenchmarkNoCache = false;
        bool BenchmarkLegacyTextures = false;
        bool BenchmarkSerialWrites = false;
        bool ValidateImporters = false;
        bool UpdateReferences = false;
        bool ShowHelp = false;
    };

    bool ParseOptions(int argc, char** argv, RunnerOptions& options, String& error);
    void PrintUsage();
    int RunSuite(const RunnerOptions& options);
    int CompareBackendDirectories(const Path& first, const Path& second, const Path& artifacts);
} // namespace Crowny::RenderTests
