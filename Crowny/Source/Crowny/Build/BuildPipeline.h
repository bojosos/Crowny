#pragma once

#include "Crowny/Build/BuildManifest.h"
#include "Crowny/Build/BuildProfile.h"
#include "Crowny/Build/ContentGraph.h"
#include "Crowny/Build/ContentPack.h"
#include "Crowny/Build/ManagedBuild.h"
#include "Crowny/Build/PlayerTemplate.h"

#include <functional>

namespace Crowny
{
    enum class BuildPipelineStage
    {
        Validate,
        ResolveContent,
        CompileManaged,
        PackContent,
        StageTemplate,
        WriteManifest,
        Publish
    };

    enum class BuildPipelineStageStatus
    {
        Pending,
        Succeeded,
        Failed,
        Cancelled,
        Skipped
    };

    const char* ToString(BuildPipelineStage stage);
    const char* ToString(BuildPipelineStageStatus status);

    struct BuildPipelineRequest
    {
        Path ProjectRoot;
        Path OutputDirectory;
        GameSettings Game;
        BuildProfile Profile;
        BuildTarget Target;
        ContentDatabase Content;
        ManagedBuildRequest Managed;
        ManagedToolchain Toolchain;
        Path TemplateRoot;
        PlayerTemplateManifest Template;
        String EngineVersion;
        String MonoVersion;
    };

    struct BuildPipelineArtifact
    {
        Path Source;
        Path RelativeDestination;
    };

    struct BuildTemplateStageRequest
    {
        Path TemplateRoot;
        PlayerTemplateManifest Template;
        PlayerTemplateRequest Validation;
        Path StageDirectory;
        Vector<BuildPipelineArtifact> Artifacts;
        BuildCancellationCheck Cancellation;
    };

    struct BuildPipelineStageReport
    {
        BuildPipelineStage Stage = BuildPipelineStage::Validate;
        BuildPipelineStageStatus Status = BuildPipelineStageStatus::Pending;
        BuildValidation Diagnostics;
    };

    struct BuildPipelineReport
    {
        String Fingerprint;
        Path OutputDirectory;
        Vector<BuildPipelineStageReport> Stages;
        bool Cancelled = false;

        bool Succeeded() const;
        const BuildPipelineStageReport* Find(BuildPipelineStage stage) const;
    };

    struct BuildPipelineOperations
    {
        std::function<BuildValidation(const BuildPipelineRequest&)> Validate;
        std::function<ContentResolveResult(const ContentDatabase&, const ContentResolveRequest&)> ResolveContent;
        std::function<ManagedCompileResult(const ManagedBuildRequest&, const ManagedToolchain&)> CompileManaged;
        std::function<String(const Path&, const ContentPackDescriptor&, const Vector<ContentPackInput>&)> PackContent;
        std::function<String(const Path&, const ContentPackDescriptor&, const Vector<ContentPackInput>&, BuildCancellationCheck)>
          PackContentCancellable;
        std::function<BuildValidation(const BuildTemplateStageRequest&)> StageTemplate;
        std::function<String(const Path&, const BuildManifest&)> WriteManifest;
        std::function<String(const Path&, const Path&)> MoveDirectory;
        // Default adapters set this so the pipeline can read back and validate
        // the concrete Crowny artifact formats. Test doubles may leave it false.
        bool ValidateProducedArtifacts = false;
    };

    BuildPipelineOperations CreateDefaultBuildPipelineOperations();

    class BuildPipelineTestAccess;

    class BuildPipeline
    {
    public:
        BuildPipeline();

        BuildPipelineReport Run(BuildPipelineRequest request, BuildCancellationCheck cancellation = {}) const;

    private:
        explicit BuildPipeline(BuildPipelineOperations operations);

        friend class BuildPipelineTestAccess;

        BuildPipelineOperations m_Operations;
    };
} // namespace Crowny
