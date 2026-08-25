#include "cwepch.h"

#include "Build/BuildManager.h"
#include "Build/PlatformInfo.h"

namespace Crowny
{
    namespace
    {
        bool MapPlatform(PlatformType source, BuildPlatform& destination)
        {
            switch (source)
            {
            case PlatformType::Windows:
                destination = BuildPlatform::WindowsX64;
                return true;
            case PlatformType::Linux:
                destination = BuildPlatform::LinuxX64;
                return true;
            case PlatformType::Mac:
            case PlatformType::MacM1:
            case PlatformType::Count:
                return false;
            }
            return false;
        }

        UUID TargetId(BuildPlatform platform)
        {
            return platform == BuildPlatform::WindowsX64 ? UUID("10000000-0000-0000-0000-000000000001")
                                                         : UUID("10000000-0000-0000-0000-000000000002");
        }

        Vector<String> ParseDefines(StringView value)
        {
            Vector<String> output;
            size_t begin = 0;
            while (begin <= value.size())
            {
                const size_t end = value.find(';', begin);
                String symbol(value.substr(begin, end == StringView::npos ? value.size() - begin : end - begin));
                if (!symbol.empty())
                    output.push_back(std::move(symbol));
                if (end == StringView::npos)
                    break;
                begin = end + 1;
            }
            std::sort(output.begin(), output.end());
            output.erase(std::unique(output.begin(), output.end()), output.end());
            return output;
        }

        PlayerTemplateRequest MakeTemplateRequest(const BuildPipelineRequest& request)
        {
            PlayerTemplateRequest validation;
            validation.EngineVersion = request.EngineVersion;
            validation.PlayerAbi = PLAYER_ABI_VERSION;
            validation.ContentSchema = CONTENT_SCHEMA_VERSION;
            validation.Platform = request.Target.Platform;
            validation.Configuration = request.Target.Configuration;
            validation.Compatibility = request.Target.Compatibility;
            validation.RequiredRenderers.clear();
            if (request.Target.Renderers != RendererPolicy::OpenGLOnly)
                validation.RequiredRenderers.push_back(RendererBackend::Vulkan);
            if (request.Target.Renderers != RendererPolicy::VulkanOnly)
                validation.RequiredRenderers.push_back(RendererBackend::OpenGL);
            return validation;
        }
    } // namespace

    BuildData::BuildData() : ActivePlatform(PlatformType::Windows)
    {
        PlatformData.resize((uint32_t)PlatformType::Count);
        for (uint32_t i = 0; i < (uint32_t)PlatformType::Count; i++)
        {
            PlatformData[i] = CreateRef<PlatformInfo>();
            PlatformData[i]->Type = (PlatformType)i;
        }

        PlatformData[(uint32_t)PlatformType::Windows]->Defines = "CROWNY_WIN;CROWNY_64;CROWNY_0_0_1;CROWNY_MONO";
        PlatformData[(uint32_t)PlatformType::Linux]->Defines = "CROWNY_LINUX;CROWNY_64;CROWNY_0_0_1;CROWNY_MONO";
        PlatformData[(uint32_t)PlatformType::Mac]->Defines = "CROWNY_MACOS;CROWNY_64;CROWNY_0_0_1;CROWNY_MONO";
        PlatformData[(uint32_t)PlatformType::MacM1]->Defines = "CROWNY_MACOS;CROWNY_ARM64;CROWNY_0_0_1;CROWNY_MONO";
        PlatformData[(uint32_t)PlatformType::Windows]->ExportSupported = true;
        PlatformData[(uint32_t)PlatformType::Linux]->ExportSupported = true;
    }

    BuildManager::BuildManager() { m_BuildData = CreateRef<BuildData>(); }

    const Vector<PlatformType>& BuildManager::GetAvailablePlatforms() const
    {
        static const Vector<PlatformType> Platforms = {
            PlatformType::Windows,
            PlatformType::Linux,
            PlatformType::Mac,
            PlatformType::MacM1,
        };
        return Platforms;
    }

    const String& BuildManager::GetDefines(PlatformType type) const
    {
        if ((uint32_t)type < (uint32_t)m_BuildData->PlatformData.size() && m_BuildData->PlatformData[(uint32_t)type])
            return m_BuildData->PlatformData[(uint32_t)type]->Defines;
        return m_BuildData->PlatformData[0]->Defines;
    }

    Ref<PlatformInfo> BuildManager::GetActivePlatformInfo() const { return m_BuildData->PlatformData[(uint32_t)m_BuildData->ActivePlatform]; }

    void BuildManager::SetActivePlatformInfo(PlatformType type)
    {
        if ((uint32_t)type < (uint32_t)PlatformType::Count)
            m_BuildData->ActivePlatform = type;
    }

    Ref<PlatformInfo> BuildManager::GetPlatformInfo(PlatformType type) const
    {
        if ((uint32_t)type < (uint32_t)m_BuildData->PlatformData.size())
            return m_BuildData->PlatformData[(uint32_t)type];
        return nullptr;
    }

    Vector<String> BuildManager::GetBaseAssemblies(PlatformType type) const
    {
        // TODO: These should ideally be paths or the project generation should change
        // since this will link the Microsoft ones.
        switch (type)
        {
        case PlatformType::Windows:
        default:
            return { "mscorlib", "System", "System.Core", "System.Collections", "System.IO", "System.Compression", "System.IO.Filesystem" };
        }
    }

    PlatformType BuildManager::GetActivePlatform() const { return m_BuildData->ActivePlatform; }

    const char* BuildManager::GetPlatformName(PlatformType type) const
    {
        switch (type)
        {
        case PlatformType::Windows:
            return "Windows";
        case PlatformType::Linux:
            return "Linux";
        case PlatformType::Mac:
            return "macOS Intel";
        case PlatformType::MacM1:
            return "macOS Apple silicon";
        default:
            return "Unknown";
        }
    }

    EditorBuildValidation BuildManager::ValidateActiveBuild(uint32_t includedAssetCount) const
    {
        EditorBuildValidation validation;
        const Ref<PlatformInfo> info = GetActivePlatformInfo();
        if (!info)
        {
            validation.Errors.push_back("The selected platform has no build configuration.");
            return validation;
        }

        if (info->OutputDirectory.empty())
            validation.Errors.push_back("Choose an output folder.");
        else if (fs::exists(info->OutputDirectory) && !fs::is_directory(info->OutputDirectory))
            validation.Errors.push_back("The output path points to a file.");
        if (info->MainScene.Empty())
            validation.Errors.push_back("Choose a main scene.");
        if (includedAssetCount == 0)
            validation.Warnings.push_back("No assets are marked for inclusion in the build.");
        if (!info->ExportSupported)
            validation.Warnings.push_back("Runtime packaging is not implemented for this platform. Crowny can build game scripts only.");
        return validation;
    }

    EditorBuildRequest BuildManager::PrepareActiveBuild(const EditorBuildInputs& inputs) const
    {
        EditorBuildRequest result;
        const Ref<PlatformInfo> info = GetActivePlatformInfo();
        if (!info)
        {
            result.Diagnostics.Error("editor.build.platform.missing", "The selected platform has no build settings.");
            return result;
        }

        BuildPlatform platform = BuildPlatform::WindowsX64;
        if (!MapPlatform(info->Type, platform))
            result.Diagnostics.Error("editor.build.platform.unsupported", "Crowny does not have a player pipeline for this platform.",
                                     GetPlatformName(info->Type));
        else if (!info->ExportSupported)
            result.Diagnostics.Error("editor.build.platform.disabled", "Player export is disabled for the selected platform.",
                                     GetPlatformName(info->Type));

        BuildPipelineRequest& request = result.Request;
        request.ProjectRoot = inputs.ProjectRoot;
        request.OutputDirectory = info->OutputDirectory;
        request.Game = inputs.Game;
        request.Profile.Schema = BUILD_PROFILE_SCHEMA;
        request.Profile.Id = UUID("20000000-0000-0000-0000-000000000001");
        request.Profile.Name = "Editor build";
        request.Profile.SceneOrder = info->MainScene.Empty() ? Vector<UUID>() : Vector<UUID>{ info->MainScene };
        request.Profile.StartupScene = info->MainScene;
        request.Profile.DefaultQuality = QualityTier::High;
        request.Target.Id = TargetId(platform);
        request.Target.Platform = platform;
        request.Target.Configuration = info->Debug ? BuildConfiguration::Development : BuildConfiguration::Shipping;
        request.Target.Symbols = ParseDefines(info->Defines);
        request.Target.DefaultQuality = request.Profile.DefaultQuality;
        request.Target.Renderers = RendererPolicy::VulkanThenOpenGL;
        request.Target.Compatibility = CompatibilityPolicy::Exact;
        request.Target.Archive = false;
        request.Target.IncludeSymbols = info->Debug;
        request.Profile.Symbols = request.Target.Symbols;
        request.Profile.Targets = { request.Target };
        request.Content = inputs.Content;
        for (const ContentAssetRecord& asset : request.Content.Assets)
            request.Profile.ContentRoots.push_back({ ContentRootKind::Asset, {}, asset.Id });
        request.Managed = inputs.Managed;
        if (request.Managed.ProjectRoot.empty())
            request.Managed.ProjectRoot = inputs.ProjectRoot;
        request.Managed.Configuration = request.Target.Configuration;
        request.Toolchain = inputs.Toolchain;
        request.TemplateRoot = inputs.TemplateRoot;
        request.Template = inputs.Template;
        request.EngineVersion = inputs.EngineVersion;
        request.MonoVersion = inputs.MonoVersion;
        if (!info->Icon.GetUUID().Empty())
        {
            if (platform == BuildPlatform::WindowsX64)
                request.Game.WindowsIcon = info->Icon.GetUUID();
            else
                request.Game.LinuxIcon = info->Icon.GetUUID();
        }

        if (inputs.ProjectRoot.empty() || !fs::is_directory(inputs.ProjectRoot))
            result.Diagnostics.Error("editor.build.project.missing", "Open a valid project before building.", inputs.ProjectRoot.string());
        if (info->OutputDirectory.empty())
            result.Diagnostics.Error("editor.build.output.missing", "Choose an output directory before building.");
        if (info->MainScene.Empty())
            result.Diagnostics.Error("editor.build.scene.missing", "Choose a startup scene before building.");
        if (!inputs.HasGameSettings)
            result.Diagnostics.Error("editor.build.game_settings.missing",
                                     "Project game settings are unavailable. Load the saved game settings before building.");
        else
            result.Diagnostics.Append(ValidateBuildProfile(request.Game, request.Profile));
        if (!inputs.HasContentDatabase)
            result.Diagnostics.Error("editor.build.content_database.missing",
                                     "The project has no cooked content database. Import and cook the selected assets before building.");
        else
            result.Diagnostics.Append(ValidateContentDatabase(request.Content));
        if (!inputs.HasTemplate || inputs.TemplateRoot.empty())
            result.Diagnostics.Error("editor.build.template.missing",
                                     "No player template is configured for this target. Build or install the matching player template.");
        else
            result.Diagnostics.Append(ValidatePlayerTemplate(request.TemplateRoot, request.Template, MakeTemplateRequest(request)));
        if (!request.Managed.Sources.empty())
        {
            for (const ManagedBuildDiagnostic& diagnostic : request.Toolchain.Diagnostics)
                result.Diagnostics.Error(diagnostic.Code, diagnostic.Message, diagnostic.Subject.string());
            if (request.Toolchain.CompilerAssembly.empty() || request.Toolchain.ReferenceDirectory.empty())
                result.Diagnostics.Error("editor.build.toolchain.missing",
                                         "No managed compiler toolchain is configured. Set CROWNY_MONO_ROOT or run the setup script.");
        }
        if (inputs.EngineVersion.empty())
            result.Diagnostics.Error("editor.build.engine_version.missing", "The running engine did not provide its build version.");
        if (inputs.MonoVersion.empty())
            result.Diagnostics.Error("editor.build.mono_version.missing", "The managed runtime version is unavailable.");
        return result;
    }

    EditorBuildReport BuildManager::ExecuteActiveBuild(const EditorBuildInputs& inputs, BuildCancellationCheck cancellation) const
    {
        EditorBuildReport result;
        EditorBuildRequest prepared = PrepareActiveBuild(inputs);
        result.Diagnostics = std::move(prepared.Diagnostics);
        if (!result.Diagnostics.IsValid())
            return result;
        result.PipelineStarted = true;
        result.Pipeline = m_Pipeline.Run(std::move(prepared.Request), std::move(cancellation));
        for (const BuildPipelineStageReport& stage : result.Pipeline.Stages)
            result.Diagnostics.Append(stage.Diagnostics);
        return result;
    }
} // namespace Crowny
