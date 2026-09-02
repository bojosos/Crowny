#include "cwepch.h"

#include "Build/BuildManager.h"
#include "Editor/Editor.h"
#include "Editor/ProjectLibrary.h"
#include "Editor/Script/ManagedProjectDependencies.h"
#include "Editor/Script/ScriptProjectGenerator.h"
#include "Editor/Script/VSCodeEditor.h"
#include "Editor/Script/VisualStudioCodeEditor.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Scripting/Managed/ManagedProgramPackage.h"

#include <algorithm>

namespace Crowny
{
    namespace
    {
        bool ResolveCoreClrApiReference(const ApplicationDesc& description, ScriptProjectReference& outReference)
        {
            Path manifest = description.Script.ProgramManifest;
            if (manifest.is_relative())
                manifest = description.WorkingDirectory / manifest;

            ManagedProgramPackageResult package = LoadManagedProgramPackage(manifest);
            if (!package.Result.Succeeded)
            {
                CW_ENGINE_WARN("Cannot generate the CoreCLR script project because the managed package is unavailable.");
                return false;
            }

            const auto host =
              std::find_if(package.Package.Program.Artifacts.begin(), package.Package.Program.Artifacts.end(), [](const auto& artifact) {
                  return artifact.Kind == ManagedProgramArtifactKind::EngineAssembly && artifact.LogicalName == "managed-host";
              });
            if (host == package.Package.Program.Artifacts.end())
            {
                CW_ENGINE_WARN("Cannot generate the CoreCLR script project because the package has no managed host artifact.");
                return false;
            }

            const Path apiAssembly = host->Filepath.parent_path() / "CrownySharp.dll";
            if (!fs::is_regular_file(apiAssembly))
            {
                CW_ENGINE_WARN("Cannot generate the CoreCLR script project because {} is missing.", apiAssembly.string());
                return false;
            }

            outReference = { "CrownySharp", apiAssembly.lexically_normal() };
            return true;
        }

        bool ResolveEngineAssemblyReference(const ApplicationDesc& description, ScriptProjectReference& outReference)
        {
            if (description.Script.Backend != ManagedBackendPreset::CoreCLR && description.EngineAssemblyPath.empty())
            {
                CW_ENGINE_WARN("Cannot generate the script solution because EngineAssemblyPath is empty.");
                return false;
            }

            Path engineAssembly = description.EngineAssemblyPath;
            if (engineAssembly.is_relative())
                engineAssembly = description.WorkingDirectory / engineAssembly;
            outReference = { CROWNY_ASSEMBLY, engineAssembly.lexically_normal() };
            return true;
        }

        void AppendFingerprintValue(String& fingerprint, const String& value)
        {
            fingerprint += std::to_string(value.size());
            fingerprint += ':';
            fingerprint += value;
        }

        void AppendFingerprintPath(String& fingerprint, const Path& path)
        {
            AppendFingerprintValue(fingerprint, path.lexically_normal().generic_string());
        }

        String MakeProjectGraphFingerprint(const CodeSolutionData& solution)
        {
            String fingerprint;
            AppendFingerprintValue(fingerprint, solution.Name);
            for (const CodeProjectData& project : solution.Projects)
            {
                AppendFingerprintValue(fingerprint, project.Name);
                AppendFingerprintPath(fingerprint, project.ProjectDirectory);
                AppendFingerprintValue(fingerprint, project.Runtime == CSharpProjectRuntime::CoreCLR ? "CoreCLR" : "Mono");
                AppendFingerprintValue(fingerprint, project.TargetFramework);
                AppendFingerprintValue(fingerprint, project.Defines);

                const auto appendPaths = [&fingerprint](const Vector<Path>& paths) {
                    Vector<Path> sortedPaths = paths;
                    std::sort(sortedPaths.begin(), sortedPaths.end(),
                              [](const Path& lhs, const Path& rhs) { return lhs.generic_string() < rhs.generic_string(); });
                    for (const Path& path : sortedPaths)
                        AppendFingerprintPath(fingerprint, path);
                };
                appendPaths(project.ScriptFiles);
                appendPaths(project.NonScriptFiles);

                const auto appendReferences = [&fingerprint](const Vector<ScriptProjectReference>& references) {
                    Vector<ScriptProjectReference> sortedReferences = references;
                    std::sort(sortedReferences.begin(), sortedReferences.end(), [](const ScriptProjectReference& lhs,
                                                                                     const ScriptProjectReference& rhs) {
                        return lhs.Name != rhs.Name ? lhs.Name < rhs.Name : lhs.Filepath.generic_string() < rhs.Filepath.generic_string();
                    });
                    for (const ScriptProjectReference& reference : sortedReferences)
                    {
                        AppendFingerprintValue(fingerprint, reference.Name);
                        AppendFingerprintPath(fingerprint, reference.Filepath);
                    }
                };
                appendReferences(project.AssemblyReferences);
                appendReferences(project.ProjectReferences);
            }
            return fingerprint;
        }

        Vector<Path> CollectProjectInputs(const CodeSolutionData& solution)
        {
            Vector<Path> paths;
            for (const CodeProjectData& project : solution.Projects)
            {
                for (const ScriptProjectReference& reference : project.AssemblyReferences)
                    if (!reference.Filepath.empty())
                        paths.push_back(reference.Filepath.lexically_normal());
                for (const ScriptProjectReference& reference : project.ProjectReferences)
                    if (!reference.Filepath.empty())
                        paths.push_back(reference.Filepath.lexically_normal());
            }
            std::sort(paths.begin(), paths.end(),
                      [](const Path& lhs, const Path& rhs) { return lhs.generic_string() < rhs.generic_string(); });
            paths.erase(std::unique(paths.begin(), paths.end(),
                                    [](const Path& lhs, const Path& rhs) { return lhs.lexically_normal() == rhs.lexically_normal(); }),
                        paths.end());
            return paths;
        }

        String MakeProjectInputFingerprint(const Vector<Path>& paths)
        {
            String fingerprint;
            for (const Path& path : paths)
            {
                AppendFingerprintPath(fingerprint, path);
                std::error_code error;
                const bool exists = fs::is_regular_file(path, error);
                AppendFingerprintValue(fingerprint, exists ? "present" : "missing");
                if (!exists)
                    continue;

                const uint64_t size = FileSystem::GetFileSize(path);
                AppendFingerprintValue(fingerprint, size != 0 ? std::to_string(size) : String());
                const fs::file_time_type writeTime = FileSystem::GetLastWriteTime(path);
                AppendFingerprintValue(
                  fingerprint, writeTime != fs::file_time_type{} ? std::to_string(writeTime.time_since_epoch().count()) : String());
            }
            return fingerprint;
        }

        void LogProjectDependencyDiagnostics(const Vector<ManagedBuildDiagnostic>& diagnostics)
        {
            for (const ManagedBuildDiagnostic& diagnostic : diagnostics)
                CW_ENGINE_WARN("Managed project dependency [{}] {}{}", diagnostic.Code, diagnostic.Message,
                               diagnostic.Subject.empty() ? String() : " (" + diagnostic.Subject.string() + ")");
        }

        bool AppendProjectDependencyReferences(CodeProjectData& project, ManagedProjectDependencyRequest request)
        {
            const ManagedProjectDependencyPlan plan = ResolveManagedProjectDependencies(request);
            if (!plan.Succeeded())
            {
                LogProjectDependencyDiagnostics(plan.Diagnostics);
                return false;
            }
            for (const ManagedProjectDependency& dependency : plan.Assemblies)
                project.AssemblyReferences.push_back({ dependency.Name, dependency.Filepath });
            return true;
        }
    } // namespace

    CodeEditorManager::CodeEditorManager() : m_ActiveEditor(nullptr)
    {
#ifdef CW_PLATFORM_WIN32
        VisualStudioCodeEditorFactory* vsCodeEditorFactory = new VisualStudioCodeEditorFactory();
        Vector<CodeEditorInstallation> vsEditors = vsCodeEditorFactory->GetAvailableEditors();
        for (const CodeEditorInstallation& editor : vsEditors)
        {
            m_FactoryPerEditor[editor.ExecutablePath] = vsCodeEditorFactory;
            m_Editors.push_back(editor);
        }
        m_Factories.push_back(vsCodeEditorFactory);
#endif

#if defined(CW_PLATFORM_WIN32) || defined(CW_PLATFORM_LINUX) || defined(CW_MACOSX)
        VSCodeEditorFactory* vscodeEditorFactory = new VSCodeEditorFactory();
        Vector<CodeEditorInstallation> vscodeEditors = vscodeEditorFactory->GetAvailableEditors();
        for (const CodeEditorInstallation& editor : vscodeEditors)
        {
            m_FactoryPerEditor[editor.ExecutablePath] = vscodeEditorFactory;
            m_Editors.push_back(editor);
        }
        m_Factories.push_back(vscodeEditorFactory);
#endif
    }

    CodeEditorManager::~CodeEditorManager()
    {
        for (auto* factory : m_Factories)
            delete factory;
        if (m_ActiveEditor != nullptr)
            delete m_ActiveEditor;
    }

    Path CodeEditorManager::GetSolutionPath() const
    {
        Path path = Editor::Get().GetProjectPath();
        path = path / (Editor::Get().GetProjectName() + ".sln");
        return path;
    }

    void CodeEditorManager::OpenFile(const Path& path, uint32_t lineNumber) const
    {
        if (m_ActiveEditor == nullptr)
            return;
        Path filepath = path;
        if (!filepath.is_absolute())
            filepath = ProjectLibrary::Get().GetAssetFolder() / filepath;

        m_ActiveEditor->OpenFile(GetSolutionPath(), filepath, lineNumber);
    }

    void CodeEditorManager::SetActive(const Path& path)
    {
        if (m_ActiveEditor != nullptr)
        {
            delete m_ActiveEditor;
            m_ActiveEditor = nullptr;
        }
        m_ActiveEditorPath.clear();
        m_LastProjectGraphFingerprint.clear();
        m_TrackedProjectInputs.clear();
        m_LastTrackedProjectInputFingerprint.clear();
        for (const CodeEditorInstallation& install : m_Editors)
        {
            if (install.ExecutablePath == path)
            {
                const auto factory = m_FactoryPerEditor.find(path);
                if (factory != m_FactoryPerEditor.end())
                {
                    m_ActiveEditor = factory->second->Create(path);
                    m_ActiveEditorPath = path;
                }
                break;
            }
        }
    }

    void CodeEditorManager::SyncSolution(const String& projectName)
    {
        const ApplicationDesc& description = Application::Get().GetApplicationDesc();
        ScriptProjectReference engineAssemblyReference;
        if (!ResolveEngineAssemblyReference(description, engineAssemblyReference))
            return;
        SyncSolution(projectName, engineAssemblyReference);
    }

    void CodeEditorManager::SyncSolution(const String& projectName, const ScriptProjectReference& engineAssemblyRef)
    {
        if (m_ActiveEditor == nullptr)
            return;

        CodeSolutionData solutionData;
        if (!BuildSolutionData(projectName, engineAssemblyRef, solutionData))
            return;
        SyncSolutionData(solutionData, true);
    }

    void CodeEditorManager::NotifyProjectInputChanged(const Path& path)
    {
        // The asset index decides whether a changed file participates in the graph.
        // This also covers new source kinds and assembly-definition formats without
        // teaching the file watcher a parallel extension list.
        if (!path.empty())
            RequestSolutionSync(GAME_ASSEMBLY);
    }

    void CodeEditorManager::NotifyProjectSettingsChanged() { RequestSolutionSync(GAME_ASSEMBLY); }

    void CodeEditorManager::Update()
    {
        if (!m_TrackedProjectInputs.empty())
        {
            const String inputFingerprint = MakeProjectInputFingerprint(m_TrackedProjectInputs);
            if (inputFingerprint != m_LastTrackedProjectInputFingerprint)
            {
                m_LastTrackedProjectInputFingerprint = inputFingerprint;
                RequestSolutionSync(GAME_ASSEMBLY);
            }
        }
        SyncIfNeeded();
    }

    void CodeEditorManager::SyncIfNeeded()
    {
        if (!m_ProjectSyncDebouncer.TryBegin())
            return;

        const String projectName = m_PendingProjectName.empty() ? String(GAME_ASSEMBLY) : m_PendingProjectName;
        m_PendingProjectName.clear();

        if (m_ActiveEditor != nullptr && Editor::Get().IsProjectLoaded())
        {
            ScriptProjectReference engineAssemblyReference;
            CodeSolutionData solutionData;
            const ApplicationDesc& description = Application::Get().GetApplicationDesc();
            if (ResolveEngineAssemblyReference(description, engineAssemblyReference) &&
                BuildSolutionData(projectName, engineAssemblyReference, solutionData))
                SyncSolutionData(solutionData, false);
        }

        m_ProjectSyncDebouncer.Complete();
    }

    void CodeEditorManager::RequestSolutionSync(const String& projectName)
    {
        if (!Editor::Get().IsProjectLoaded())
            return;
        m_PendingProjectName = projectName;
        m_ProjectSyncDebouncer.Notify();
    }

    bool CodeEditorManager::BuildSolutionData(const String& projectName, const ScriptProjectReference& engineAssemblyRef,
                                              CodeSolutionData& outData) const
    {
        outData = {};
        outData.Name = Editor::Get().GetProjectName();

        const Vector<AssetType> assetTypes = { AssetType::ScriptCode, AssetType::PlainText, AssetType::Shader };
        const Vector<Ref<LibraryEntry>> codeEntries = ProjectLibrary::Get().Search("*", assetTypes);
        const ApplicationDesc& description = Application::Get().GetApplicationDesc();
        const bool usesCoreClr = description.Script.Backend == ManagedBackendPreset::CoreCLR;

        CodeProjectData& codeProjectData = outData.Projects.emplace_back();
        codeProjectData.Name = projectName;
        codeProjectData.ProjectDirectory = Editor::Get().GetProjectPath();
        codeProjectData.Runtime = usesCoreClr ? CSharpProjectRuntime::CoreCLR : CSharpProjectRuntime::Mono;
        ManagedProjectDependencyRequest dependencyRequest;
        dependencyRequest.ProjectRoot = codeProjectData.ProjectDirectory;
        dependencyRequest.DeclaredAssemblies = Editor::Get().GetProjectSettings()->ManagedAssemblyReferences;
        if (usesCoreClr)
        {
            ScriptProjectReference coreClrApiReference;
            if (!ResolveCoreClrApiReference(description, coreClrApiReference))
                return false;
            codeProjectData.AssemblyReferences.push_back(std::move(coreClrApiReference));
            dependencyRequest.ExcludedAssemblies = { codeProjectData.AssemblyReferences.front().Filepath };
            dependencyRequest.ReservedAssemblyNames = { "CrownySharp" };
            if (!dependencyRequest.DeclaredAssemblies.empty())
            {
                dependencyRequest.SearchDirectories = { codeProjectData.AssemblyReferences.front().Filepath.parent_path() };
                const DotNetSdk sdk = LocateDotNetSdk(description.WorkingDirectory / ".deps" / "dotnet");
                dependencyRequest.FrameworkDirectories = FindDotNetFrameworkReferenceDirectories(sdk, codeProjectData.TargetFramework);
                if (dependencyRequest.FrameworkDirectories.empty())
                {
                    dependencyRequest.ResolveClosure = false;
                    CW_ENGINE_WARN(
                      "The CoreCLR reference pack is unavailable; generated project dependencies will be fully validated when scripts are rebuilt.");
                }
            }
        }
        else
        {
            const PlatformType activePlatform = BuildManager::Get().GetActivePlatform();
            codeProjectData.Defines = BuildManager::Get().GetDefines(activePlatform);
            codeProjectData.AssemblyReferences.push_back(engineAssemblyRef);
            for (const String& assemblyName : BuildManager::Get().GetBaseAssemblies(activePlatform))
                codeProjectData.AssemblyReferences.push_back({ assemblyName, {} });

            dependencyRequest.ExcludedAssemblies = { engineAssemblyRef.Filepath };
            dependencyRequest.ReservedAssemblyNames = { CROWNY_ASSEMBLY };
            ManagedToolchain toolchain = LocateManagedToolchain(description.Script.RuntimeRoot);
            if (toolchain.IsValid())
            {
                dependencyRequest.SearchDirectories = { engineAssemblyRef.Filepath.parent_path() };
                dependencyRequest.FrameworkDirectories = { toolchain.ReferenceDirectory };
            }
            else
                dependencyRequest.ResolveClosure = false;
        }

        if (!AppendProjectDependencyReferences(codeProjectData, std::move(dependencyRequest)))
            return false;

        for (const Ref<LibraryEntry>& entry : codeEntries)
        {
            if (entry->Type != LibraryEntryType::File)
                continue;

            FileEntry* fileEntry = static_cast<FileEntry*>(entry.get());
            if (fileEntry->Metadata->Type == AssetType::ScriptCode)
            {
                const Ref<CSharpScriptImportOptions> scriptImportOptions =
                  StaticRefCast<CSharpScriptImportOptions>(fileEntry->Metadata->ImportOptions);
                bool isEditorScript = false;
                if (scriptImportOptions != nullptr)
                    isEditorScript = scriptImportOptions->IsEditorScript;
                if (!isEditorScript)
                    codeProjectData.ScriptFiles.push_back(fileEntry->Filepath);
            }
            else
                codeProjectData.NonScriptFiles.push_back(fileEntry->Filepath);
        }

        return true;
    }

    bool CodeEditorManager::SyncSolutionData(const CodeSolutionData& data, bool force)
    {
        if (m_ActiveEditor == nullptr)
            return false;

        const String fingerprint = MakeProjectGraphFingerprint(data);
        if (!force && fingerprint == m_LastProjectGraphFingerprint)
            return true;

        const CodeEditorSyncResult result = m_ActiveEditor->Sync(data, Editor::Get().GetProjectPath());
        if (!result.Succeeded)
            return false;

        m_LastProjectGraphFingerprint = fingerprint;
        CaptureProjectInputs(data);
        if (result.Changed)
            m_ActiveEditor->ReloadSolution(data, Editor::Get().GetProjectPath());
        return true;
    }

    void CodeEditorManager::CaptureProjectInputs(const CodeSolutionData& data)
    {
        m_TrackedProjectInputs = CollectProjectInputs(data);
        m_LastTrackedProjectInputFingerprint = MakeProjectInputFingerprint(m_TrackedProjectInputs);
    }

    void CodeEditorManager::SetEditorExecutablePath(const Path& path)
    {
        if (m_ActiveEditor == nullptr)
            return;

        m_ActiveEditor->SetEditorExecutablePath(path);
    }

} // namespace Crowny
