#include "cwepch.h"

#include "Editor/EditorLayer.h"

#include "Crowny/Application/EngineRuntime.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Build/ManagedBuild.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/Input/InputMapSerialization.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/PlatformUtils.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Common/Version.h"
#include "Crowny/Events/ImGuiEvent.h"
#include "Crowny/ImGui/ImGuiMenu.h"
#include "Crowny/Physics/Physics2D.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/EnvironmentMap.h"
#include "Crowny/Scene/Prefab.h"
#include "Crowny/Scene/SceneRenderer.h"
#include "Crowny/Scene/ScriptRuntime.h"
#include "Crowny/Scripting/Managed/ManagedProgramPackage.h"
#include "Crowny/Scripting/Managed/ManagedScripting.h"
#include "Crowny/Scripting/ManagedReload.h"
#include "Crowny/Serialization/SceneSerializer.h"

#include "Editor/PrefabUtils.h"

#include "Panels/AssetBrowserPanel.h"
#include "Panels/AudioMixerPanel.h"
#include "Panels/ConsolePanel.h"
#include "Panels/EditorPanelRegistry.h"
#include "Panels/EntityInspector.h"
#include "Panels/HierarchyPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/ViewportPanel.h"
#ifdef CW_WITH_NODES
#include "Panels/NodeEditor/NodeEditorPanel.h"
#endif

#include "Crowny/NodeGraph/BuiltinNodeTypes.h"

#include "Editor/ColliderOverlay.h"
#include "Editor/Editor.h"
#include "Editor/EditorAssets.h"
#include "Editor/EditorScenePersistence.h"
#include "Editor/ProjectLibrary.h"
#include "Editor/Settings/EditorSettingsPersistence.h"
#include "Serialization/ProjectSettingsSerializer.h"
#include "UI/Properties.h"
#include "UI/UIUtils.h"

#include "Crowny/Renderer/Font.h"

#include "Build/BuildManager.h"
#include "Build/BuildSceneSelection.h"
#include "Editor/Script/CodeEditor.h"
#include "Editor/Script/ManagedProjectDependencies.h"
#include "Editor/Script/ScriptProjectGenerator.h"

#ifdef CW_PLATFORM_WIN32
#include "Editor/Script/VisualStudioCodeEditor.h"
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <spdlog/fmt/fmt.h>

#include "Crowny/ImGui/ImGuiVulkanTexture.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

namespace Crowny
{
    namespace
    {
        String EscapeXml(String value)
        {
            value = StringUtils::Replace(value, "&", "&amp;");
            value = StringUtils::Replace(value, "\"", "&quot;");
            value = StringUtils::Replace(value, "<", "&lt;");
            value = StringUtils::Replace(value, ">", "&gt;");
            return value;
        }

        Vector<Path> CollectGameScriptFiles()
        {
            Vector<Path> scripts;
            const Vector<Ref<LibraryEntry>> entries = ProjectLibrary::Get().Search("*", { AssetType::ScriptCode });
            for (const Ref<LibraryEntry>& entry : entries)
            {
                if (entry->Type != LibraryEntryType::File)
                    continue;
                const auto* file = static_cast<const FileEntry*>(entry.get());
                const Ref<CSharpScriptImportOptions> options = StaticRefCast<CSharpScriptImportOptions>(file->Metadata->ImportOptions);
                if (options == nullptr || !options->IsEditorScript)
                    scripts.push_back(file->Filepath);
            }
            std::sort(scripts.begin(), scripts.end());
            scripts.erase(std::unique(scripts.begin(), scripts.end()), scripts.end());
            return scripts;
        }

        const ManagedProgramArtifact* FindManagedArtifact(const ManagedProgramDefinition& program, ManagedProgramArtifactKind kind,
                                                          StringView logicalName)
        {
            const auto artifact = std::find_if(program.Artifacts.begin(), program.Artifacts.end(), [&](const ManagedProgramArtifact& value) {
                return value.Kind == kind && value.LogicalName == logicalName;
            });
            return artifact != program.Artifacts.end() ? &*artifact : nullptr;
        }

        bool WriteCoreClrGameProject(const Path& projectFile, const Vector<Path>& scripts, const Path& apiAssembly,
                                     const Vector<ManagedProjectDependency>& dependencies)
        {
            StringStream sourceItems;
            for (const Path& script : scripts)
                sourceItems << "    <Compile Include=\"" << EscapeXml(fs::absolute(script).generic_string()) << "\" />\n";

            StringStream referenceItems;
            const auto writeReference = [&](StringView name, const Path& assembly) {
                referenceItems << "    <Reference Include=\"" << EscapeXml(String(name)) << "\">\n"
                               << "      <HintPath>" << EscapeXml(fs::absolute(assembly).generic_string()) << "</HintPath>\n"
                               << "      <Private>true</Private>\n"
                               << "    </Reference>\n";
            };
            writeReference("CrownySharp", apiAssembly);
            for (const ManagedProjectDependency& dependency : dependencies)
                writeReference(dependency.Name, dependency.Filepath);

            const String project = "<Project Sdk=\"Microsoft.NET.Sdk\">\n"
                                   "  <PropertyGroup>\n"
                                   "    <TargetFramework>net10.0</TargetFramework>\n"
                                   "    <AssemblyName>GameAssembly</AssemblyName>\n"
                                   "    <RootNamespace>GameAssembly</RootNamespace>\n"
                                   "    <EnableDefaultCompileItems>false</EnableDefaultCompileItems>\n"
                                   "    <Nullable>disable</Nullable>\n"
                                   "    <Deterministic>true</Deterministic>\n"
                                   "    <GenerateDependencyFile>true</GenerateDependencyFile>\n"
                                   "  </PropertyGroup>\n"
                                   "  <ItemGroup>\n" +
                                   sourceItems.str() +
                                   "  </ItemGroup>\n"
                                   "  <ItemGroup>\n" +
                                   referenceItems.str() +
                                   "  </ItemGroup>\n"
                                   "</Project>\n";
            return FileSystem::WriteTextFile(projectFile, project);
        }

        void LogManagedBuildDiagnostics(const Vector<ManagedBuildDiagnostic>& diagnostics)
        {
            for (const ManagedBuildDiagnostic& diagnostic : diagnostics)
                CW_ENGINE_ERROR("Managed build [{}] {}{}", diagnostic.Code, diagnostic.Message,
                                diagnostic.Subject.empty() ? String() : " (" + diagnostic.Subject.string() + ")");
        }

        bool StageMonoDependencies(const Vector<ManagedProjectDependency>& dependencies, const Path& stagingDirectory,
                                   Vector<ManagedProgramArtifact>& artifacts, Vector<Path>& stagedDependencies)
        {
            if (dependencies.empty())
                return true;

            const Path dependencyDirectory = stagingDirectory / "Dependencies";
            std::error_code error;
            fs::create_directories(dependencyDirectory, error);
            if (error)
            {
                CW_ENGINE_ERROR("Could not create managed dependency staging directory {}: {}", dependencyDirectory.string(), error.message());
                return false;
            }

            for (const ManagedProjectDependency& dependency : dependencies)
            {
                const Path destination = dependencyDirectory / dependency.Filepath.filename();
                String publishError;
                if (!PublishManagedArtifact(dependency.Filepath, destination, &publishError))
                {
                    CW_ENGINE_ERROR("Could not stage managed dependency {}: {}", dependency.Filepath.string(), publishError);
                    return false;
                }
                artifacts.push_back({ ManagedProgramArtifactKind::DependencyAssembly, dependency.Name, destination });
                stagedDependencies.push_back(destination);
            }
            return true;
        }

        Vector<Path> CollectCoreClrRuntimeDependencies(const Path& stagingDirectory, const Path& gameAssembly)
        {
            Vector<Path> dependencies;
            std::error_code error;
            for (const fs::directory_entry& entry : fs::directory_iterator(stagingDirectory, error))
            {
                if (error)
                    break;
                String extension = entry.path().extension().string();
                StringUtils::ToLower(extension);
                if (!entry.is_regular_file(error) || extension != ".dll")
                    continue;
                if (entry.path().lexically_normal() != gameAssembly.lexically_normal())
                    dependencies.push_back(entry.path());
            }
            std::sort(dependencies.begin(), dependencies.end());
            dependencies.erase(std::unique(dependencies.begin(), dependencies.end()), dependencies.end());
            return dependencies;
        }

        bool PublishManagedDependencies(const Vector<Path>& dependencies, const Path& destinationDirectory, String& error)
        {
            if (dependencies.empty())
                return true;

            std::error_code filesystemError;
            fs::create_directories(destinationDirectory, filesystemError);
            if (filesystemError)
            {
                error = filesystemError.message();
                return false;
            }
            for (const Path& dependency : dependencies)
                if (!PublishManagedArtifact(dependency, destinationDirectory / dependency.filename(), &error))
                    return false;
            return true;
        }

        constexpr const char* SCENE_EXTENSION = ".cwscene";

        bool HasSceneExtension(const Path& path)
        {
            String extension = path.extension().string();
            StringUtils::ToLower(extension);
            return extension == SCENE_EXTENSION;
        }

        // Appends the scene extension only when the path does not already carry it, so "Level.cwscene" never turns
        // into "Level.cwscene.cwscene" and a dotted name such as "Level 1.5" keeps its stem.
        Path EnsureSceneExtension(const Path& path)
        {
            if (HasSceneExtension(path))
                return path;
            Path result = path;
            result += SCENE_EXTENSION;
            return result;
        }

        Vector<BuildSceneOption> CollectBuildSceneOptions(const Vector<UUID>& sceneIds, const Path& assetFolder)
        {
            Vector<BuildSceneOption> scenes;
            for (const UUID& id : sceneIds)
            {
                Path path;
                UUID currentId;
                if (ProjectLibrary::Get().TryGetSourcePath(id, AssetType::Scene, path) &&
                    ProjectLibrary::Get().TryGetAssetId(path, AssetType::Scene, currentId) && currentId == id)
                    scenes.push_back({ id, path });
            }
            return MakeBuildSceneOptions(std::move(scenes), assetFolder);
        }
    } // namespace

    void EditorLayer::SetProjectSettings()
    {
        Ref<ProjectSettings> projSettings = Editor::Get().GetProjectSettings();
        s_EditorCamera.SetPosition(projSettings->EditorCameraPosition);
        s_EditorCamera.SetFocalPoint(projSettings->EditorCameraFocalPoint);
        s_EditorCamera.SetPitch(projSettings->EditorCameraRotation.x);
        s_EditorCamera.SetYaw(projSettings->EditorCameraRotation.y);
        s_EditorCamera.SetDistance(projSettings->EditorCameraDistance);
        m_Temp = nullptr;
        m_TempSceneId = UUID::EMPTY;
        if (!projSettings->LastOpenSceneId.Empty())
            OpenScene(projSettings->LastOpenSceneId);
        // Switching projects must never keep the previous project's scene active. When the new project has no
        // (valid) last-open scene, queue an empty scene so the deferred swap in OnUpdate replaces and unloads the
        // old one, clears undo history and resets the hierarchy selection through the scene lifecycle listener.
        if (m_Temp == nullptr)
            CreateNewScene();
        m_ViewportPanel->SetGizmoMode(projSettings->GizmoMode);
        m_ViewportPanel->SetGizmoLocalMode(projSettings->GizmoLocalMode);

        if (m_Temp != nullptr)
        {
            Entity selected = m_Temp->TryGetEntityFromUuid(projSettings->LastSelectedEntityID);
            m_HierarchyPanel->SetSelectedEntity(selected ? selected : m_Temp->GetRootEntity());
        }

        m_HierarchyPanel->SetHierarchy(projSettings->ExpandedEntities);
    }

    void EditorLayer::SaveProjectSettings()
    {
        Ref<ProjectSettings> projSettings = Editor::Get().GetProjectSettings();

        projSettings->EditorCameraPosition = s_EditorCamera.GetPosition();
        projSettings->EditorCameraFocalPoint = s_EditorCamera.GetFocalPoint();
        projSettings->EditorCameraRotation = { s_EditorCamera.GetPitch(), s_EditorCamera.GetYaw() };
        projSettings->EditorCameraDistance = s_EditorCamera.GetDistance();
        const Ref<Scene>& activeScene = SceneManager::TryGet()->GetActiveScene();
        projSettings->LastOpenSceneId = activeScene ? SceneManager::TryGet()->GetActiveSceneId() : UUID::EMPTY;
        projSettings->LastAssetBrowserSelectedEntry = m_AssetBrowser->GetCurrentEntryPath();

        projSettings->GizmoMode = m_ViewportPanel->GetGizmoMode();
        projSettings->GizmoLocalMode = m_ViewportPanel->GetGizmoLocalMode();

        if (m_HierarchyPanel->GetSelectedEntity())
            projSettings->LastSelectedEntityID = m_HierarchyPanel->GetSelectedEntity().GetUuid();
        projSettings->ExpandedEntities = m_HierarchyPanel->GetSerializableHierarchy();
    }

    void EditorLayer::ApplyEditorSettings()
    {
        Ref<EditorSettings> editorSettings = Editor::Get().GetEditorSettings();
        m_ShowDemoWindow = editorSettings->ShowImGuiDemoWindow;
        m_ShowColliders = editorSettings->ShowPhysicsColliders;
        m_AutoLoadLastProject = editorSettings->AutoLoadLastProject;
        m_ShowScriptDebugInfo = editorSettings->ShowScriptDebugInfo;
        m_ShowEntityDebugInfo = editorSettings->ShowEntityDebugInfo;

        m_WireframeMode = editorSettings->WireframeMode;
        m_ShowRenderingStatistics = editorSettings->ShowRenderingStatistics;
        m_ShowGrid = editorSettings->ShowGrid;
        m_ShowGridAxes = editorSettings->ShowGridAxes;
        m_GridFineSize = editorSettings->GridFineSize;
        m_GridCoarseSize = editorSettings->GridCoarseSize;
        m_GridLineWidth = editorSettings->GridLineWidth;
        m_GridOpacity = editorSettings->GridOpacity;
        m_ColliderColor = editorSettings->ColliderColor;

        m_ConsolePanel->SetMessageLevelEnabled(ConsoleBuffer::Message::Level::Info, editorSettings->EnableConsoleInfoMessages);
        m_ConsolePanel->SetMessageLevelEnabled(ConsoleBuffer::Message::Level::Warn, editorSettings->EnableConsoleWarningMessages);
        m_ConsolePanel->SetMessageLevelEnabled(ConsoleBuffer::Message::Level::Error, editorSettings->EnableConsoleErrorMessages);

        m_ConsolePanel->SetCollapseEnabled(editorSettings->CollapseConsole);
        m_ConsolePanel->SetScrollToBottomEnabled(editorSettings->ScrollToBottom);

        if (m_AutoLoadLastProject)
            m_PendingProjectPath = SelectStartupProject(*editorSettings, [](const Path& path) { return fs::is_directory(path); });
        if (!m_LaunchOptions.Project.empty())
            m_PendingProjectPath = m_LaunchOptions.Project;
        m_LaunchScenePending = !m_LaunchOptions.Scene.empty();
        m_LaunchPlayPending = m_LaunchOptions.Play;
    }

    void EditorLayer::FinishDeferredStartup()
    {
        if (!m_DeferredStartupPending)
            return;
        m_DeferredStartupPending = false;

        Renderer2D::Init();
        ForwardRenderer::Init();
        CreateRenderTarget();
        m_SceneRenderer->SetRenderTarget(m_RenderTarget);
        m_SceneRenderer->Init();

        Application::Get().GetRuntime().StartRuntimeServices();

        if (!m_PendingProjectPath.empty())
        {
            Editor::Get().LoadProject(m_PendingProjectPath);
            SetProjectSettings();
            m_AssetBrowser->Initialize();
            m_PendingProjectPath.clear();
            // Files that appeared while the editor was closed have no metadata yet and the file watcher only reports
            // changes made from now on; scan the asset folder once so they import in the background like dropped files.
            if (ProjectLibrary::IsStartedUp() && fs::exists(ProjectLibrary::Get().GetAssetFolder()))
                ProjectLibrary::Get().RefreshAsync(ProjectLibrary::Get().GetAssetFolder());
        }

        CodeEditorManager::StartUp();
        const Ref<EditorSettings> editorSettings = Editor::Get().GetEditorSettings();
        if (!editorSettings->CodeEditorPath.empty() && fs::exists(editorSettings->CodeEditorPath))
            CodeEditorManager::Get().SetActive(editorSettings->CodeEditorPath);
        else
        {
            const Vector<CodeEditorInstallation>& installations = CodeEditorManager::Get().GetAvailableEditors();
            if (!installations.empty())
                CodeEditorManager::Get().SetActive(installations.front().ExecutablePath);
        }

        BuildManager::StartUp();
        CodeEditorManager::Get().SyncSolution(GAME_ASSEMBLY);
    }

    void EditorLayer::BuildGame()
    {
        m_ShowBuildWindow = true;
        if (m_PlayerBuild.valid() || !BuildManager::IsStartedUp())
            return;
        m_BuildStatus = BuildStatus::Ready;
        m_BuildProgress = 0.0f;
        m_BuildResult.clear();
        m_BuildProjectRoot = Editor::Get().GetProjectPath();
        const auto settings = Editor::Get().GetProjectSettings();
        BuildManager::Get().SetActivePlatformInfo(PlatformType::Windows);
        const auto info = BuildManager::Get().GetActivePlatformInfo();
        info->MainScene = settings->GameStartupScene;
        info->Debug = settings->GameBuildDevelopment;
        info->OutputDirectory =
          settings->GameBuildOutput.empty()
            ? m_BuildProjectRoot.parent_path() / (m_BuildProjectRoot.filename().string() + "-Build") / "Windows"
            : (settings->GameBuildOutput.is_absolute() ? settings->GameBuildOutput : m_BuildProjectRoot / settings->GameBuildOutput);
    }

    void EditorLayer::StartPlayerBuild()
    {
        if (m_PlayerBuild.valid())
            return;
        try
        {
            EditorBuildInputs inputs;
            inputs.ProjectRoot = Editor::Get().GetProjectPath();
            Path startupScene;
            if (!ProjectLibrary::Get().TryGetSourcePath(BuildManager::Get().GetActivePlatformInfo()->MainScene, AssetType::Scene, startupScene))
                throw std::runtime_error("Choose a saved startup scene.");
            const auto startup = YAML::LoadFile(startupScene.string());
            bool hasCamera = false;
            if (const auto entities = startup["Entities"]; entities && entities.IsSequence())
                for (const auto& entity : entities)
                    hasCamera = hasCamera || entity["CameraComponent"].IsDefined();
            if (!hasCamera)
                throw std::runtime_error("Add a Camera component to the startup scene and save it before building.");
            inputs.Game.ProductName = Editor::Get().GetProjectName();
            inputs.Game.ArtifactName = SanitizeArtifactName(inputs.Game.ProductName);
            inputs.HasGameSettings = true;
            inputs.Content = ProjectLibrary::Get().GetBuildContentDatabase();
            const Path runtimeSettings = inputs.ProjectRoot / "Internal/Build/Game.yaml";
            fs::create_directories(runtimeSettings.parent_path());
            YAML::Emitter runtime;
            runtime << YAML::BeginMap;
            SerializeInputMap(Editor::Get().GetProjectSettings()->InputActions, runtime);
            runtime << YAML::EndMap;
            String settingsError;
            if (!FileSystem::WriteTextFileAtomic(runtimeSettings, runtime.c_str(), &settingsError))
                throw std::runtime_error(settingsError);
            inputs.Content.Assets.push_back({ UUID("ffffffff-ffff-ffff-ffff-000000000001"),
                                              "Settings/Game.yaml",
                                              runtimeSettings.lexically_relative(inputs.ProjectRoot),
                                              {},
                                              "Settings",
                                              {} });
            inputs.HasContentDatabase = true;
            const auto& description = Application::Get().GetApplicationDesc();
            inputs.Managed.Sources = CollectGameScriptFiles();
            inputs.Toolchain =
              LocateManagedToolchain(description.Script.Backend == ManagedBackendPreset::Mono ? description.Script.RuntimeRoot : Path());
#ifdef CW_PLATFORM_WIN32
            wchar_t executable[32768];
            const DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
            inputs.TemplateRoot = Path(std::wstring(executable, length)).parent_path().parent_path() / "PlayerTemplate";
#endif
            if (!fs::is_regular_file(inputs.TemplateRoot / "Game.exe"))
                inputs.TemplateRoot = inputs.TemplateRoot.parent_path().parent_path() / "Release-windows-x86_64/PlayerTemplate";
            if (!fs::is_regular_file(inputs.TemplateRoot / "Game.exe"))
                throw std::runtime_error(
                  "Player runtime is missing. Run Scripts\\crowny.bat build Editor --configuration Release, then restart the editor.");
            inputs.Managed.References = { inputs.TemplateRoot / "Managed/CrownySharp.dll" };
            ManagedProjectDependencyRequest dependencies;
            dependencies.ProjectRoot = inputs.ProjectRoot;
            dependencies.DeclaredAssemblies = Editor::Get().GetProjectSettings()->ManagedAssemblyReferences;
            dependencies.SearchDirectories = { inputs.TemplateRoot / "Managed" };
            dependencies.FrameworkDirectories = { inputs.Toolchain.ReferenceDirectory };
            dependencies.ExcludedAssemblies = inputs.Managed.References;
            dependencies.ReservedAssemblyNames = { CROWNY_ASSEMBLY };
            const auto resolved = ResolveManagedProjectDependencies(dependencies);
            if (!resolved.Succeeded())
                throw std::runtime_error(resolved.Diagnostics.front().Message);
            for (const auto& dependency : resolved.Assemblies)
                inputs.Managed.References.push_back(dependency.Filepath);
            inputs.EngineVersion = CROWNY_VERSION_STRING;
            inputs.MonoVersion = "6.12";
            inputs.Template.EngineVersion = inputs.EngineVersion;
            inputs.Template.Platform = BuildPlatform::WindowsX64;
            inputs.Template.Configuration =
              BuildManager::Get().GetActivePlatformInfo()->Debug ? BuildConfiguration::Development : BuildConfiguration::Shipping;
            inputs.Template.Renderers = { RendererBackend::Vulkan, RendererBackend::OpenGL };
            const auto info = BuildManager::Get().GetActivePlatformInfo();
            m_BuiltGamePath = (info->OutputDirectory.is_absolute() ? info->OutputDirectory : inputs.ProjectRoot / info->OutputDirectory) / "Game.exe";
            m_BuildStatus = BuildStatus::Running;
            m_BuildProgress = 0.1f;
            m_BuildResult = "Compiling scripts and packaging the standalone game...";
            // Snapshot all editor-owned data before leaving the main thread.
            PlatformInfo platform = *info;
            if (platform.OutputDirectory.is_relative())
                platform.OutputDirectory = inputs.ProjectRoot / platform.OutputDirectory;
            m_BuildCancellation = std::make_shared<std::atomic<bool>>(false);
            m_PlayerBuild = std::async(
              std::launch::async, [inputs = std::move(inputs), platform, cancellation = m_BuildCancellation]() mutable -> std::pair<bool, String> {
                  try
                  {
                      if (const String error =
                            PlayerTemplateStore::CreateManifest(inputs.TemplateRoot, inputs.Template, { "Game.exe" }, inputs.Template);
                          !error.empty())
                          return { false, error };
                      inputs.HasTemplate = true;
                      BuildManager manager;
                      *manager.GetActivePlatformInfo() = platform;
                      const auto report = manager.ExecuteActiveBuild(inputs, [cancellation] { return cancellation->load(); });
                      String message;
                      for (const auto& issue : report.Diagnostics.Issues)
                          message += issue.Message + (issue.Subject.empty() ? "" : " (" + issue.Subject + ")") + "\n";
                      if (report.Succeeded())
                          message = "Standalone game built at " + report.Pipeline.OutputDirectory.string();
                      return { report.Succeeded(), message };
                  }
                  catch (const std::exception& error)
                  {
                      return { false, error.what() };
                  }
              });
        }
        catch (const std::exception& error)
        {
            m_BuildStatus = BuildStatus::Failed;
            m_BuildResult = error.what();
        }
    }

    void EditorLayer::UI_BuildGame()
    {
        if (m_PlayerBuild.valid() && m_PlayerBuild.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            auto [success, message] = m_PlayerBuild.get();
            m_BuildStatus = success ? BuildStatus::Succeeded : BuildStatus::Failed;
            m_BuildProgress = 1.0f;
            m_BuildResult = std::move(message);
            AddNotification(success ? "Standalone game built." : "Game build failed.", success ? NotificationKind::Success : NotificationKind::Error);
        }
        if (!m_ShowBuildWindow)
            return;
        if (m_BuildProjectRoot != Editor::Get().GetProjectPath() && !m_PlayerBuild.valid())
            BuildGame();

        ImGui::SetNextWindowSize(ImVec2(620.0f, 560.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Build game", &m_ShowBuildWindow))
        {
            ImGui::End();
            return;
        }

        if (!BuildManager::IsStartedUp() || !Editor::Get().IsProjectLoaded())
        {
            ImGui::TextDisabled("Open a project to build a game.");
            ImGui::End();
            return;
        }

        if (m_PlayerBuild.valid() && m_BuildProjectRoot != Editor::Get().GetProjectPath())
        {
            ImGui::TextWrapped("Building %s. Reopen Build game after it finishes to configure this project.",
                               m_BuildProjectRoot.filename().string().c_str());
            ImGui::End();
            return;
        }
        BuildManager& buildManager = BuildManager::Get();
        Ref<PlatformInfo> platformInfo = buildManager.GetActivePlatformInfo();
        const Vector<PlatformType> platforms = { PlatformType::Windows };
        const Vector<BuildSceneOption> sceneOptions =
          CollectBuildSceneOptions(ProjectLibrary::Get().GetAllAssets(AssetType::Scene), ProjectLibrary::Get().GetAssetFolder());
        const auto findSceneOption = [&sceneOptions](const UUID& sceneId) {
            return std::find_if(sceneOptions.begin(), sceneOptions.end(),
                                [&sceneId](const BuildSceneOption& option) { return option.Id == sceneId; });
        };

        ImGui::TextUnformatted("Build setup");
        ImGui::TextDisabled("Build a standalone Windows game with its content and runtime included.");
        ImGui::Spacing();

        ImGui::BeginDisabled(m_PlayerBuild.valid());
        if (ImGui::BeginTable("##BuildSettings", 2,
                              ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_NoPadOuterX))
        {
            ImGui::TableSetupColumn("Setting", ImGuiTableColumnFlags_WidthFixed, 150.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Platform");
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##BuildPlatform", buildManager.GetPlatformName(buildManager.GetActivePlatform())))
            {
                for (PlatformType platform : platforms)
                {
                    const bool selected = platform == buildManager.GetActivePlatform();
                    if (ImGui::Selectable(buildManager.GetPlatformName(platform), selected))
                    {
                        buildManager.SetActivePlatformInfo(platform);
                        platformInfo = buildManager.GetActivePlatformInfo();
                        if (platformInfo->OutputDirectory.empty())
                            platformInfo->OutputDirectory = Editor::Get().GetProjectPath().parent_path() /
                                                            (Editor::Get().GetProjectName() + "-Build") /
                                                            buildManager.GetPlatformName(platformInfo->Type);
                        m_BuildStatus = BuildStatus::Ready;
                        m_BuildResult.clear();
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Output folder");
            ImGui::TableSetColumnIndex(1);
            String outputPath = platformInfo ? platformInfo->OutputDirectory.string() : String();
            const float browseWidth = ImGui::CalcTextSize("Browse").x + ImGui::GetStyle().FramePadding.x * 2.0f;
            ImGui::SetNextItemWidth(std::max(80.0f, ImGui::GetContentRegionAvail().x - browseWidth - ImGui::GetStyle().ItemSpacing.x));
            if (ImGui::InputTextWithHint("##BuildOutput", "Choose a folder outside the project", &outputPath) && platformInfo)
                platformInfo->OutputDirectory = outputPath;
            ImGui::SameLine();
            if (ImGui::Button("Browse"))
            {
                Vector<Path> paths;
                const Path initialPath =
                  platformInfo && !platformInfo->OutputDirectory.empty() ? platformInfo->OutputDirectory : Editor::Get().GetProjectPath();
                if (FileSystem::OpenFileDialog(FileDialogType::OpenFolder, paths, "Choose build output", initialPath) && !paths.empty())
                    platformInfo->OutputDirectory = paths.front();
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Main scene");
            ImGui::TableSetColumnIndex(1);
            const char* scenePreview = "None";
            String selectedSceneName;
            if (platformInfo && !platformInfo->MainScene.Empty())
            {
                const auto selectedOption = findSceneOption(platformInfo->MainScene);
                selectedSceneName =
                  selectedOption != sceneOptions.end() ? selectedOption->DisplayName : "Missing scene: " + platformInfo->MainScene.ToString();
                if (!selectedSceneName.empty())
                    scenePreview = selectedSceneName.c_str();
            }
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##BuildMainScene", scenePreview))
            {
                if (sceneOptions.empty())
                    ImGui::TextDisabled("No scene assets found.");
                for (const BuildSceneOption& option : sceneOptions)
                {
                    const bool selected = platformInfo && option.Id == platformInfo->MainScene;
                    ImGui::PushID(option.Id.ToString().c_str());
                    if (ImGui::Selectable(option.DisplayName.c_str(), selected) && platformInfo && !selected)
                    {
                        platformInfo->MainScene = option.Id;
                    }
                    ImGui::PopID();
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", option.SourcePath.string().c_str());
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Configuration");
            ImGui::TableSetColumnIndex(1);
            if (platformInfo)
                ImGui::Checkbox("Script debug symbols", &platformInfo->Debug);

            ImGui::EndTable();
        }

        const auto activeScene = SceneManager::Get().GetActiveScene();
        const bool canSave = activeScene && CanSaveEditorScene(SceneManager::Get().GetExecutionState());
        ImGui::BeginDisabled(!canSave);
        if (ImGui::Button("Save and use current scene"))
        {
            if (SaveActiveScene())
            {
                UUID id;
                if (ProjectLibrary::Get().TryGetAssetId(activeScene->GetFilepath(), AssetType::Scene, id))
                    platformInfo->MainScene = id;
            }
        }
        ImGui::EndDisabled();
        ImGui::TextWrapped("Build uses scenes saved on disk. Use the button above to save and select the open scene.");
        ImGui::TextWrapped("All imported project assets and scenes are included, including assets loaded by scripts.");
        const auto settings = Editor::Get().GetProjectSettings();
        const Path relativeOutput = platformInfo->OutputDirectory.lexically_relative(Editor::Get().GetProjectPath());
        const Path savedOutput = !relativeOutput.empty() && IsSafeRelativeBuildPath(relativeOutput) ? relativeOutput : platformInfo->OutputDirectory;
        if (settings->GameStartupScene != platformInfo->MainScene || settings->GameBuildOutput != savedOutput ||
            settings->GameBuildDevelopment != platformInfo->Debug)
        {
            settings->GameStartupScene = platformInfo->MainScene;
            settings->GameBuildOutput = savedOutput;
            settings->GameBuildDevelopment = platformInfo->Debug;
            Editor::Get().SaveProjectSettings();
        }
        ImGui::EndDisabled();

        EditorBuildValidation validation = buildManager.ValidateActiveBuild(1);
        if (platformInfo && !platformInfo->MainScene.Empty() && findSceneOption(platformInfo->MainScene) == sceneOptions.end())
            validation.Errors.push_back("The selected main scene no longer exists.");
        if (!validation.Errors.empty() || !validation.Warnings.empty())
        {
            ImGui::Spacing();
            for (const String& error : validation.Errors)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.35f, 0.32f, 1.0f));
                ImGui::BulletText("%s", error.c_str());
                ImGui::PopStyleColor();
            }
            for (const String& warning : validation.Warnings)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.72f, 0.28f, 1.0f));
                ImGui::BulletText("%s", warning.c_str());
                ImGui::PopStyleColor();
            }
        }

        if (m_BuildStatus != BuildStatus::Ready)
        {
            ImGui::Spacing();
            ImGui::ProgressBar(m_BuildProgress, ImVec2(-FLT_MIN, 0.0f));
            if (!m_BuildResult.empty())
            {
                const ImVec4 statusColor = m_BuildStatus == BuildStatus::Succeeded ? ImVec4(0.35f, 0.82f, 0.48f, 1.0f)
                                           : m_BuildStatus == BuildStatus::Running ? ImGui::GetStyleColorVec4(ImGuiCol_Text)
                                                                                   : ImVec4(0.95f, 0.35f, 0.32f, 1.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, statusColor);
                ImGui::TextWrapped("%s", m_BuildResult.c_str());
                ImGui::PopStyleColor();
            }
        }

        ImGui::Spacing();
        const bool canBuild = validation.IsValid() && m_BuildStatus != BuildStatus::Running && !ProjectLibrary::Get().IsImporting() &&
                              CanSaveEditorScene(SceneManager::Get().GetExecutionState());
        ImGui::BeginDisabled(!canBuild);
        if (ImGui::Button("Build game", ImVec2(-FLT_MIN, 0.0f)))
        {
            Editor::Get().SaveProject();
            StartPlayerBuild();
        }
        ImGui::EndDisabled();
        if (!canBuild && ProjectLibrary::Get().IsImporting())
            ImGui::TextDisabled("Wait for asset import to finish.");
        if (!CanSaveEditorScene(SceneManager::Get().GetExecutionState()))
            ImGui::TextDisabled("Stop Play or Simulate before building.");

        if (m_PlayerBuild.valid() && ImGui::Button("Cancel build"))
            m_BuildCancellation->store(true);

        if (m_BuildStatus == BuildStatus::Succeeded && fs::is_regular_file(m_BuiltGamePath))
        {
            if (ImGui::Button("Run game"))
                PlatformUtils::OpenExternally(m_BuiltGamePath);
            ImGui::SameLine();
            if (ImGui::Button("Open build folder"))
                PlatformUtils::ShowInExplorer(m_BuiltGamePath);
        }

        ImGui::End();
    }

    bool EditorLayer::RebuildAssemblies()
    {
        Application* application = Application::TryGet();
        ManagedScripting* managed = application != nullptr ? application->GetRuntime().GetManagedScripting() : nullptr;
        if (application == nullptr || managed == nullptr || !managed->IsStarted())
        {
            CW_ENGINE_ERROR("Managed runtime is unavailable; game scripts cannot be built or loaded.");
            return false;
        }
        const ApplicationDesc& description = application->GetApplicationDesc();
        const Path assemblyDirectory = Editor::Get().GetProjectPath() / INTERNAL_ASSEMBLY_PATH;
        if (m_ManagedBuildGeneration == std::numeric_limits<uint64_t>::max())
        {
            CW_ENGINE_ERROR("Managed build generations are exhausted for this editor process.");
            return false;
        }
        m_ManagedBuildGeneration = std::max<uint64_t>(m_ManagedBuildGeneration + 1, std::max<uint64_t>(m_AssemblyReloadDebouncer.GetGeneration(), 2));
        const uint64_t generation = m_ManagedBuildGeneration;
        const Path stagingDirectory = assemblyDirectory / ".staging" / std::to_string(generation);
        std::error_code fsError;
        fs::remove_all(stagingDirectory, fsError);
        fsError.clear();
        fs::create_directories(stagingDirectory, fsError);
        if (fsError)
        {
            CW_ENGINE_ERROR("Could not create managed assembly staging directory {0}: {1}", stagingDirectory.string(), fsError.message());
            return false;
        }

        Vector<Path> scripts = CollectGameScriptFiles();
        if (scripts.empty())
        {
            const Path emptyAssemblySource = stagingDirectory / "EmptyGameAssembly.g.cs";
            if (!FileSystem::WriteTextFile(emptyAssemblySource, "// Generated for a project with no gameplay scripts.\n"))
            {
                CW_ENGINE_ERROR("Could not write the empty managed game assembly source.");
                return false;
            }
            scripts.push_back(emptyAssemblySource);
        }
        const Path stagedGameAssembly = stagingDirectory / "GameAssembly.dll";
        const Path stagedGameDependencies = stagingDirectory / "GameAssembly.deps.json";
        Vector<Path> stagedRuntimeDependencies;
        ManagedProgramDefinition program;
        program.Generation = generation;

        if (description.Script.Backend == ManagedBackendPreset::CoreCLR)
        {
            Path manifest = description.Script.ProgramManifest;
            if (manifest.is_relative())
                manifest = description.WorkingDirectory / manifest;
            ManagedProgramPackageResult package = LoadManagedProgramPackage(manifest, generation);
            if (!package.Result.Succeeded)
            {
                for (const ManagedDiagnostic& diagnostic : package.Result.Diagnostics)
                    CW_ENGINE_ERROR("Managed package [{}]: {}", diagnostic.Code, diagnostic.Message);
                return false;
            }
            const ManagedProgramArtifact* hostAssembly =
              FindManagedArtifact(package.Package.Program, ManagedProgramArtifactKind::EngineAssembly, "managed-host");
            const Path apiAssembly = hostAssembly != nullptr ? hostAssembly->Filepath.parent_path() / "CrownySharp.dll" : Path();
            if (!fs::is_regular_file(apiAssembly))
            {
                CW_ENGINE_ERROR("The CoreCLR package does not contain the CrownySharp API assembly beside its managed host.");
                return false;
            }

            DotNetSdk sdk = LocateDotNetSdk(description.WorkingDirectory / ".deps" / "dotnet");
            if (!sdk.IsValid())
            {
                LogManagedBuildDiagnostics(sdk.Diagnostics);
                return false;
            }

            ManagedProjectDependencyRequest dependencyRequest;
            dependencyRequest.ProjectRoot = Editor::Get().GetProjectPath();
            dependencyRequest.DeclaredAssemblies = Editor::Get().GetProjectSettings()->ManagedAssemblyReferences;
            dependencyRequest.SearchDirectories = { apiAssembly.parent_path() };
            dependencyRequest.FrameworkDirectories = FindDotNetFrameworkReferenceDirectories(sdk);
            dependencyRequest.ExcludedAssemblies = { apiAssembly };
            dependencyRequest.ReservedAssemblyNames = { "CrownySharp" };
            if (!dependencyRequest.DeclaredAssemblies.empty() && dependencyRequest.FrameworkDirectories.empty())
            {
                LogManagedBuildDiagnostics(
                  { { "MPD107", "The .NET reference pack for net10.0 could not be found beside the selected SDK.", sdk.Executable } });
                return false;
            }
            const ManagedProjectDependencyPlan dependencyPlan = ResolveManagedProjectDependencies(dependencyRequest);
            if (!dependencyPlan.Succeeded())
            {
                LogManagedBuildDiagnostics(dependencyPlan.Diagnostics);
                return false;
            }

            const Path projectFile = stagingDirectory / "GameAssembly.Managed.csproj";
            if (!WriteCoreClrGameProject(projectFile, scripts, apiAssembly, dependencyPlan.Assemblies))
            {
                CW_ENGINE_ERROR("Could not write the SDK-style game project at {}.", projectFile.string());
                return false;
            }
            ManagedSdkBuildRequest request;
            request.ProjectFile = projectFile;
            request.OutputDirectory = stagingDirectory;
            ManagedSdkBuildResult result = BuildManagedSdkProject(request, sdk);
            if (!result.Succeeded())
            {
                LogManagedBuildDiagnostics(result.Diagnostics);
                if (!result.StandardOutput.empty())
                    CW_ENGINE_ERROR("{}", result.StandardOutput);
                if (!result.StandardError.empty())
                    CW_ENGINE_ERROR("{}", result.StandardError);
                return false;
            }
            stagedRuntimeDependencies = CollectCoreClrRuntimeDependencies(stagingDirectory, stagedGameAssembly);
            program = std::move(package.Package.Program);
            program.Generation = generation;
            std::erase_if(program.Artifacts, [](const ManagedProgramArtifact& artifact) {
                return artifact.Kind == ManagedProgramArtifactKind::GameAssembly ||
                       (artifact.Kind == ManagedProgramArtifactKind::DependencyManifest && artifact.LogicalName == "game");
            });
            program.Artifacts.push_back({ ManagedProgramArtifactKind::GameAssembly, "game", stagedGameAssembly });
            program.Artifacts.push_back({ ManagedProgramArtifactKind::DependencyManifest, "game", stagedGameDependencies });
        }
        else if (description.Script.Backend == ManagedBackendPreset::Mono)
        {
            Path engineAssemblyPath = description.EngineAssemblyPath;
            if (engineAssemblyPath.is_relative())
                engineAssemblyPath = description.WorkingDirectory / engineAssemblyPath;
            ManagedToolchain toolchain = LocateManagedToolchain(description.Script.RuntimeRoot);

            ManagedProjectDependencyRequest dependencyRequest;
            dependencyRequest.ProjectRoot = Editor::Get().GetProjectPath();
            dependencyRequest.DeclaredAssemblies = Editor::Get().GetProjectSettings()->ManagedAssemblyReferences;
            dependencyRequest.SearchDirectories = { engineAssemblyPath.parent_path() };
            dependencyRequest.FrameworkDirectories = { toolchain.ReferenceDirectory };
            dependencyRequest.ExcludedAssemblies = { engineAssemblyPath };
            dependencyRequest.ReservedAssemblyNames = { CROWNY_ASSEMBLY };
            const ManagedProjectDependencyPlan dependencyPlan = ResolveManagedProjectDependencies(dependencyRequest);
            if (!dependencyPlan.Succeeded())
            {
                LogManagedBuildDiagnostics(dependencyPlan.Diagnostics);
                return false;
            }

            ManagedBuildRequest request;
            request.ProjectRoot = Editor::Get().GetProjectPath();
            request.OutputAssembly = stagedGameAssembly;
            request.Sources = scripts;
            request.References = { engineAssemblyPath };
            for (const ManagedProjectDependency& dependency : dependencyPlan.Assemblies)
                request.References.push_back(dependency.Filepath);
            request.Symbols = { "CROWNY_MONO" };
            ManagedCompileResult result = CompileManagedAssembly(request, toolchain);
            if (!result.Succeeded())
            {
                LogManagedBuildDiagnostics(result.Diagnostics);
                if (!result.StandardOutput.empty())
                    CW_ENGINE_ERROR("{}", result.StandardOutput);
                if (!result.StandardError.empty())
                    CW_ENGINE_ERROR("{}", result.StandardError);
                return false;
            }
            program.Artifacts = { { ManagedProgramArtifactKind::EngineAssembly, CROWNY_ASSEMBLY, engineAssemblyPath } };
            if (!StageMonoDependencies(dependencyPlan.Assemblies, stagingDirectory, program.Artifacts, stagedRuntimeDependencies))
                return false;
            program.Artifacts.push_back({ ManagedProgramArtifactKind::GameAssembly, GAME_ASSEMBLY, stagedGameAssembly });
        }
        else
        {
            CW_ENGINE_ERROR("The selected managed backend cannot rebuild scripts inside the editor.");
            return false;
        }

        if (!fs::is_regular_file(stagedGameAssembly) ||
            (description.Script.Backend == ManagedBackendPreset::CoreCLR && !fs::is_regular_file(stagedGameDependencies)))
        {
            CW_ENGINE_ERROR("The managed build completed without producing its required game artifacts.");
            return false;
        }

        if (m_InspectorPanel)
            m_InspectorPanel->ResetUndoTransactions(false);
        ManagedOperationResult reloaded = managed->ReloadProgram(program);
        if (!reloaded.Succeeded)
        {
            for (const ManagedDiagnostic& diagnostic : reloaded.Diagnostics)
                CW_ENGINE_ERROR("Managed reload [{}]: {}", diagnostic.Code, diagnostic.Message);
            return false;
        }

        const Path gameAssemblyPath = assemblyDirectory / "GameAssembly.dll";
        String publishError;
        if (!PublishManagedAssembly(stagedGameAssembly, gameAssemblyPath, &publishError))
        {
            CW_ENGINE_WARN("Reloaded game scripts, but could not publish the last-good assembly: {0}", publishError);
            return true;
        }

        if (description.Script.Backend == ManagedBackendPreset::CoreCLR &&
            !PublishManagedArtifact(stagedGameDependencies, assemblyDirectory / stagedGameDependencies.filename(), &publishError))
            CW_ENGINE_WARN("Reloaded game scripts, but could not publish the dependency manifest: {0}", publishError);

        const Path dependencyDestination =
          description.Script.Backend == ManagedBackendPreset::Mono ? assemblyDirectory / "Dependencies" : assemblyDirectory;
        if (description.Script.Backend == ManagedBackendPreset::Mono)
        {
            // This directory is generated exclusively from the validated dependency plan. Replace it so removed references cannot reappear
            // after a later domain restart.
            std::error_code dependencyCleanupError;
            fs::remove_all(dependencyDestination, dependencyCleanupError);
            if (dependencyCleanupError)
                CW_ENGINE_WARN("Reloaded game scripts, but could not remove stale managed dependencies: {0}", dependencyCleanupError.message());
        }
        if (!PublishManagedDependencies(stagedRuntimeDependencies, dependencyDestination, publishError))
            CW_ENGINE_WARN("Reloaded game scripts, but could not publish a managed dependency: {0}", publishError);

        CW_ENGINE_INFO("Reloaded game scripts from {0}", gameAssemblyPath.string());
        return true;
    }

    void EditorLayer::CreateNewScene()
    {
        m_Temp = CreateRef<Scene>("Scene");
        m_TempSceneId = UUID::EMPTY;
        m_Temp->SetEditorScene(true);
        const String title = "Crowny Editor - " + Editor::Get().GetProjectName() + " - " + m_Temp->GetName();
        Application::TryGet()->GetWindow().SetTitle(title);
    }

    void EditorLayer::OpenScene()
    {
        Vector<Path> outPaths;
        if (FileSystem::OpenFileDialog(FileDialogType::OpenFile, outPaths, "Open Scene", ProjectLibrary::Get().GetAssetFolder(),
                                       { Editor::GetSceneDialogFilter() }))
            OpenScene(EnsureSceneExtension(outPaths[0]));
    }

    void EditorLayer::OpenScene(const Path& filepath)
    {
        const Path sourcePath = filepath.lexically_normal();
        if (!AssetFileSystemScanner::IsPathWithin(ProjectLibrary::Get().GetAssetFolder(), sourcePath) || !fs::is_regular_file(sourcePath))
        {
            AddNotification("Scenes must be opened from the project Assets folder.", NotificationKind::Error);
            return;
        }

        UUID sceneId;
        if (!ProjectLibrary::Get().TryGetAssetId(sourcePath, AssetType::Scene, sceneId))
        {
            ProjectLibrary::Get().Refresh(sourcePath);
            if (!ProjectLibrary::Get().TryGetAssetId(sourcePath, AssetType::Scene, sceneId))
            {
                AddNotification(fmt::format("Could not import {0} as a scene asset.", sourcePath.filename().string()), NotificationKind::Error);
                return;
            }
        }
        OpenScene(sceneId);
    }

    void EditorLayer::OpenScene(const UUID& sceneId)
    {
        Path sourcePath;
        if (!ProjectLibrary::Get().TryGetSourcePath(sceneId, AssetType::Scene, sourcePath))
        {
            AddNotification("The scene asset is missing or is no longer a scene.", NotificationKind::Error);
            return;
        }

        Ref<Scene> scene = CreateRef<Scene>(sourcePath.string(), false);
        scene->SetEditorScene(true);
        SceneSerializer serializer(scene);
        if (!serializer.Deserialize(sourcePath))
        {
            AddNotification(fmt::format("Could not open {0}.", sourcePath.filename().string()), NotificationKind::Error);
            return;
        }
        m_Temp = std::move(scene);
        m_TempSceneId = sceneId;
        AddRecentScene(sceneId);
    }

    bool EditorLayer::SaveActiveSceneAs()
    {
        SceneManager* sceneManager = SceneManager::TryGet();
        if (sceneManager == nullptr || !CanSaveEditorScene(sceneManager->GetExecutionState()))
        {
            AddNotification("Stop Play or Simulate before saving the scene.");
            return false;
        }

        Vector<Path> outPaths;
        if (FileSystem::OpenFileDialog(FileDialogType::SaveFile, outPaths, "Save scene", ProjectLibrary::Get().GetAssetFolder(),
                                       { Editor::GetSceneDialogFilter() }) &&
            !outPaths.empty())
        {
            const Path path = EnsureSceneExtension(outPaths[0]).lexically_normal();
            if (!AssetFileSystemScanner::IsPathWithin(ProjectLibrary::Get().GetAssetFolder(), path))
            {
                AddNotification("Scenes must be saved inside the project Assets folder.", NotificationKind::Error);
                return false;
            }
            const auto& scene = SceneManager::TryGet()->GetActiveScene();
            if (!scene)
                return false;
            scene->SetImGuiLayout(Application::TryGet()->GetImGuiLayer()->SaveLayout());
            SceneSerializer serializer(scene);
            if (!serializer.Serialize(path))
            {
                AddNotification("Could not save the scene. See the console for details.", NotificationKind::Error);
                return false;
            }
            if (!SynchronizeActiveSceneAsset(scene))
                return false;
            const String title = "Crowny Editor - " + Editor::Get().GetProjectName() + " - " + SceneManager::TryGet()->GetActiveScene()->GetName();
            Application::TryGet()->GetWindow().SetTitle(title);
            AddNotification(fmt::format("Saved {0}.", path.filename().string()), NotificationKind::Success);
            return true;
        }
        return false;
    }

    bool EditorLayer::SaveActiveScene()
    {
        SceneManager* sceneManager = SceneManager::TryGet();
        if (sceneManager == nullptr || !CanSaveEditorScene(sceneManager->GetExecutionState()))
        {
            AddNotification("Stop Play or Simulate before saving the scene.");
            return false;
        }

        const auto& scene = sceneManager->GetActiveScene();
        if (!scene)
            return false;
        if (scene->GetFilepath().empty())
            return SaveActiveSceneAs();
        else
        {
            scene->SetImGuiLayout(Application::TryGet()->GetImGuiLayer()->SaveLayout());
            SceneSerializer serializer(scene);
            if (!serializer.Serialize(scene->GetFilepath()))
            {
                AddNotification("Could not save the scene. See the console for details.", NotificationKind::Error);
                return false;
            }
            if (!SynchronizeActiveSceneAsset(scene))
                return false;
            const String title = "Crowny Editor - " + Editor::Get().GetProjectName() + " - " + scene->GetName();
            Application::TryGet()->GetWindow().SetTitle(title);
            AddNotification(fmt::format("Saved {0}.", scene->GetFilepath().filename().string()), NotificationKind::Success);
            return true;
        }
        return false;
    }

    bool EditorLayer::SynchronizeActiveSceneAsset(const Ref<Scene>& scene)
    {
        if (!scene || scene->GetFilepath().empty())
            return false;
        ProjectLibrary::Get().Refresh(scene->GetFilepath());

        UUID sceneId;
        if (!ProjectLibrary::Get().TryGetAssetId(scene->GetFilepath(), AssetType::Scene, sceneId))
        {
            AddNotification("The scene was written, but its asset identity could not be updated.", NotificationKind::Error);
            return false;
        }

        SceneManager* sceneManager = SceneManager::TryGet();
        const UUID previousSceneId = sceneManager->GetActiveSceneId();
        if (previousSceneId != sceneId)
        {
            sceneManager->SetActiveScene(scene, sceneId);
            if (!previousSceneId.Empty())
                sceneManager->UnloadScene(previousSceneId);
        }
        Editor::Get().GetProjectSettings()->LastOpenSceneId = sceneId;
        AddRecentScene(sceneId);
        return true;
    }

    void EditorLayer::AddRecentScene(const UUID& sceneId) { ProjectSettingsSerializer::AddRecentScene(*Editor::Get().GetProjectSettings(), sceneId); }

    void EditorLayer::ExecuteProjectAssetRefresh()
    {
        Vector<Path> queueCopy;
        {
            Lock lock(m_FileWatchMutex);
            m_FileWatchQueue.swap(queueCopy);
        }

        // This is detecting the .meta? yikes
        for (const Path& path : queueCopy)
            ProjectLibrary::Get().Refresh(path);
    }

} // namespace Crowny
