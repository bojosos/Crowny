#pragma once

#include "Build/PlatformInfo.h"
#include "Crowny/Build/BuildPipeline.h"
#include "Crowny/Common/Module.h"

namespace Crowny
{

    struct BuildData : public RefCounted
    {
        BuildData();

        PlatformType ActivePlatform;
        Vector<Ref<PlatformInfo>> PlatformData;
    };

    struct EditorBuildValidation
    {
        Vector<String> Errors;
        Vector<String> Warnings;

        bool IsValid() const { return Errors.empty(); }
    };

    struct EditorBuildInputs
    {
        Path ProjectRoot;
        GameSettings Game;
        bool HasGameSettings = false;
        ContentDatabase Content;
        bool HasContentDatabase = false;
        ManagedBuildRequest Managed;
        ManagedToolchain Toolchain;
        Path TemplateRoot;
        PlayerTemplateManifest Template;
        bool HasTemplate = false;
        String EngineVersion;
        String MonoVersion;
    };

    struct EditorBuildRequest
    {
        BuildPipelineRequest Request;
        BuildValidation Diagnostics;

        bool IsValid() const { return Diagnostics.IsValid(); }
    };

    struct EditorBuildReport
    {
        BuildValidation Diagnostics;
        BuildPipelineReport Pipeline;
        bool PipelineStarted = false;

        bool Succeeded() const { return Diagnostics.IsValid() && PipelineStarted && Pipeline.Succeeded(); }
    };

    class BuildManager : public Module<BuildManager>
    {
    public:
        BuildManager();

        const Vector<PlatformType>& GetAvailablePlatforms() const;
        void SetActivePlatformInfo(PlatformType type);

        Ref<PlatformInfo> GetActivePlatformInfo() const;
        Ref<PlatformInfo> GetPlatformInfo(PlatformType type) const;
        Vector<String> GetBaseAssemblies(PlatformType type) const;
        const String& GetDefines(PlatformType platform) const;
        PlatformType GetActivePlatform() const;
        const char* GetPlatformName(PlatformType type) const;
        EditorBuildValidation ValidateActiveBuild(uint32_t includedAssetCount) const;
        EditorBuildRequest PrepareActiveBuild(const EditorBuildInputs& inputs) const;
        EditorBuildReport ExecuteActiveBuild(const EditorBuildInputs& inputs, BuildCancellationCheck cancellation = {}) const;

    private:
        Ref<BuildData> m_BuildData;
        BuildPipeline m_Pipeline;
    };
} // namespace Crowny
