#include "cwepch.h"

#include "Editor/EditorLayer.h"

#include "Crowny/Application/EngineRuntime.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Build/ManagedBuild.h"
#include "Crowny/Import/Importer.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/PlatformUtils.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Events/ImGuiEvent.h"
#include "Crowny/ImGui/ImGuiMenu.h"
#include "Crowny/Physics/Physics2D.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/EnvironmentMap.h"
#include "Crowny/Scene/Prefab.h"
#include "Crowny/Scene/SceneRenderer.h"
#include "Crowny/Scene/ScriptRuntime.h"
#include "Crowny/Scripting/ManagedReload.h"
#include "Crowny/Scripting/Managed/ManagedProgramPackage.h"
#include "Crowny/Scripting/Managed/ManagedScripting.h"
#include "Crowny/Serialization/SceneSerializer.h"

#include "Editor/PrefabUtils.h"

#include "Panels/AssetBrowserPanel.h"
#include "Panels/AudioMixerPanel.h"
#include "Panels/ComponentEditor.h"
#include "Panels/ConsolePanel.h"
#include "Panels/EditorPanelRegistry.h"
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
#include "Serialization/ProjectSettingsSerializer.h"
#include "UI/Properties.h"
#include "UI/UIUtils.h"

#include "Crowny/Renderer/Font.h"

#include "Build/BuildManager.h"
#include "Editor/Script/CodeEditor.h"
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
                const Ref<CSharpScriptImportOptions> options =
                  StaticRefCast<CSharpScriptImportOptions>(file->Metadata->ImportOptions);
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

        bool WriteCoreClrGameProject(const Path& projectFile, const Vector<Path>& scripts, const Path& apiAssembly)
        {
            StringStream sourceItems;
            for (const Path& script : scripts)
                sourceItems << "    <Compile Include=\"" << EscapeXml(fs::absolute(script).generic_string()) << "\" />\n";

            const String project =
              "<Project Sdk=\"Microsoft.NET.Sdk\">\n"
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
              "  <ItemGroup>\n"
              "    <Reference Include=\"CrownySharp\">\n"
              "      <HintPath>" +
              EscapeXml(fs::absolute(apiAssembly).generic_string()) +
              "</HintPath>\n"
              "      <Private>true</Private>\n"
              "    </Reference>\n"
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

        if (m_AutoLoadLastProject && !editorSettings->LastOpenProject.empty())
            m_PendingProjectPath = editorSettings->LastOpenProject;
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
        }

        CodeEditorManager::StartUp();
        const Ref<EditorSettings> editorSettings = Editor::Get().GetEditorSettings();
        if (editorSettings->CodeEditorPath.extension() == ".exe" && fs::exists(editorSettings->CodeEditorPath))
            CodeEditorManager::Get().SetActive(editorSettings->CodeEditorPath);
        else
        {
            const Vector<CodeEditorInstallation>& installations = CodeEditorManager::Get().GetAvailableEditors();
            if (!installations.empty())
                CodeEditorManager::Get().SetActive(installations.front().ExecutablePath);
        }

        BuildManager::StartUp();
        const ApplicationDesc& applicationDesc = Application::TryGet()->GetApplicationDesc();
        Path engineAssemblyPath = applicationDesc.EngineAssemblyPath;
        if (engineAssemblyPath.empty())
        {
            CW_ENGINE_WARN("Cannot add the engine assembly to the generated script solution because EngineAssemblyPath is empty.");
            return;
        }

        if (engineAssemblyPath.is_relative())
            engineAssemblyPath = applicationDesc.WorkingDirectory / engineAssemblyPath;
        CodeEditorManager::Get().SyncSolution(GAME_ASSEMBLY, { CROWNY_ASSEMBLY, engineAssemblyPath.lexically_normal() });
    }

    void EditorLayer::BuildGame()
    {
        m_ShowBuildWindow = true;
        m_BuildStatus = BuildStatus::Ready;
        m_BuildProgress = 0.0f;
        m_BuildResult.clear();

        if (!BuildManager::IsStartedUp())
            return;

        const Ref<PlatformInfo> info = BuildManager::Get().GetActivePlatformInfo();
        if (info && info->OutputDirectory.empty())
            info->OutputDirectory = Editor::Get().GetProjectPath() / "Build" / BuildManager::Get().GetPlatformName(info->Type);
    }

    void EditorLayer::UI_BuildGame()
    {
        if (!m_ShowBuildWindow)
            return;

        ImGui::SetNextWindowSize(ImVec2(620.0f, 560.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Build game", &m_ShowBuildWindow))
        {
            ImGui::End();
            return;
        }

        if (!BuildManager::IsStartedUp())
        {
            ImGui::TextDisabled("Build tools are still starting.");
            ImGui::End();
            return;
        }

        BuildManager& buildManager = BuildManager::Get();
        Ref<PlatformInfo> platformInfo = buildManager.GetActivePlatformInfo();
        const Vector<PlatformType>& platforms = buildManager.GetAvailablePlatforms();
        Vector<UUID> sceneIds = ProjectLibrary::Get().GetAllAssets(AssetType::Scene);
        Vector<Ref<FileEntry>> includedAssets = ProjectLibrary::Get().GetAssetsForBuild();

        ImGui::TextUnformatted("Build setup");
        ImGui::TextDisabled("Configure the player target and compile the project's game scripts.");
        ImGui::Spacing();

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
                            platformInfo->OutputDirectory =
                              Editor::Get().GetProjectPath() / "Build" / buildManager.GetPlatformName(platformInfo->Type);
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
            if (ImGui::InputTextWithHint("##BuildOutput", "Choose an output folder", &outputPath) && platformInfo)
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
                selectedSceneName = ProjectLibrary::Get().GetAssetName(platformInfo->MainScene);
                if (!selectedSceneName.empty())
                    scenePreview = selectedSceneName.c_str();
            }
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##BuildMainScene", scenePreview))
            {
                if (sceneIds.empty())
                    ImGui::TextDisabled("No scene assets found.");
                for (const UUID& sceneId : sceneIds)
                {
                    const String sceneName = ProjectLibrary::Get().GetAssetName(sceneId);
                    const bool selected = platformInfo && sceneId == platformInfo->MainScene;
                    if (ImGui::Selectable(sceneName.c_str(), selected) && platformInfo)
                    {
                        platformInfo->MainScene = sceneId;
                        ProjectLibrary::Get().SetIncludeInBuild(ProjectLibrary::Get().UuidToPath(sceneId), true);
                        includedAssets = ProjectLibrary::Get().GetAssetsForBuild();
                    }
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
                ImGui::Checkbox("Debug symbols", &platformInfo->Debug);

            ImGui::EndTable();
        }

        ImGui::Spacing();
        if (ImGui::CollapsingHeader(fmt::format("Included assets ({0})", includedAssets.size()).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            const float assetListHeight = std::min(130.0f, ImGui::GetTextLineHeightWithSpacing() * (includedAssets.size() + 1.0f));
            ImGui::BeginChild("##IncludedAssets", ImVec2(0.0f, std::max(46.0f, assetListHeight)), true);
            if (includedAssets.empty())
                ImGui::TextDisabled("Mark assets as included from the asset browser or choose a main scene.");
            for (const Ref<FileEntry>& asset : includedAssets)
            {
                ImGui::TextUnformatted(asset->ElementName.c_str());
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", asset->Filepath.string().c_str());
            }
            ImGui::EndChild();
        }

        EditorBuildValidation validation = buildManager.ValidateActiveBuild((uint32_t)includedAssets.size());
        if (platformInfo && !platformInfo->MainScene.Empty() &&
            std::find(sceneIds.begin(), sceneIds.end(), platformInfo->MainScene) == sceneIds.end())
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
                const ImVec4 statusColor =
                  m_BuildStatus == BuildStatus::Succeeded ? ImVec4(0.35f, 0.82f, 0.48f, 1.0f) : ImVec4(0.95f, 0.35f, 0.32f, 1.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, statusColor);
                ImGui::TextWrapped("%s", m_BuildResult.c_str());
                ImGui::PopStyleColor();
            }
        }

        ImGui::Spacing();
        const bool canBuild = validation.IsValid() && m_BuildStatus != BuildStatus::Running && !ProjectLibrary::Get().IsImporting();
        ImGui::BeginDisabled(!canBuild);
        if (ImGui::Button("Build scripts", ImVec2(-FLT_MIN, 0.0f)))
        {
            m_BuildStatus = BuildStatus::Running;
            m_BuildProgress = 0.15f;
            m_BuildResult = "Saving project and compiling game scripts...";

            SaveActiveScene();
            Editor::Get().SaveProject();
            m_BuildProgress = 0.45f;
            const bool built = RebuildAssemblies();
            m_BuildProgress = 1.0f;
            m_BuildStatus = built ? BuildStatus::Succeeded : BuildStatus::Failed;
            m_BuildResult = built ? "Game scripts built. Runtime packaging is not available, so no player was created."
                                  : "Game script compilation failed. Check the console for compiler output.";
            AddNotification(built ? "Game scripts built." : "Game script build failed.", built ? NotificationKind::Success : NotificationKind::Error);
        }
        ImGui::EndDisabled();
        if (!canBuild && ProjectLibrary::Get().IsImporting())
            UI::SetTooltip("Wait for asset import to finish.");

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
        m_ManagedBuildGeneration =
          std::max<uint64_t>(m_ManagedBuildGeneration + 1, std::max<uint64_t>(m_AssemblyReloadDebouncer.GetGeneration(), 2));
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
            const Path projectFile = stagingDirectory / "GameAssembly.Managed.csproj";
            if (!WriteCoreClrGameProject(projectFile, scripts, apiAssembly))
            {
                CW_ENGINE_ERROR("Could not write the SDK-style game project at {}.", projectFile.string());
                return false;
            }
            DotNetSdk sdk = LocateDotNetSdk(description.WorkingDirectory / ".deps" / "dotnet");
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
            ManagedBuildRequest request;
            request.ProjectRoot = Editor::Get().GetProjectPath();
            request.OutputAssembly = stagedGameAssembly;
            request.Sources = scripts;
            request.References = { engineAssemblyPath };
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
            program.Artifacts = {
                { ManagedProgramArtifactKind::EngineAssembly, CROWNY_ASSEMBLY, engineAssemblyPath },
                { ManagedProgramArtifactKind::GameAssembly, GAME_ASSEMBLY, stagedGameAssembly },
            };
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
            OpenScene(outPaths[0].replace_extension(".cwscene"));
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

    void EditorLayer::SaveActiveSceneAs()
    {
        SceneManager* sceneManager = SceneManager::TryGet();
        if (sceneManager == nullptr || !CanSaveEditorScene(sceneManager->GetExecutionState()))
        {
            AddNotification("Stop Play or Simulate before saving the scene.");
            return;
        }

        Vector<Path> outPaths;
        if (FileSystem::OpenFileDialog(FileDialogType::SaveFile, outPaths, "Save scene", ProjectLibrary::Get().GetAssetFolder(),
                                       { Editor::GetSceneDialogFilter() }))
        {
            const Path path = outPaths[0].replace_extension(".cwscene").lexically_normal();
            if (!AssetFileSystemScanner::IsPathWithin(ProjectLibrary::Get().GetAssetFolder(), path))
            {
                AddNotification("Scenes must be saved inside the project Assets folder.", NotificationKind::Error);
                return;
            }
            const auto& scene = SceneManager::TryGet()->GetActiveScene();
            if (!scene)
                return;
            scene->SetImGuiLayout(Application::TryGet()->GetImGuiLayer()->SaveLayout());
            SceneSerializer serializer(scene);
            serializer.Serialize(path);
            if (!SynchronizeActiveSceneAsset(scene))
                return;
            const String title = "Crowny Editor - " + Editor::Get().GetProjectName() + " - " + SceneManager::TryGet()->GetActiveScene()->GetName();
            Application::TryGet()->GetWindow().SetTitle(title);
            AddNotification(fmt::format("Saved {0}.", path.filename().string()), NotificationKind::Success);
        }
    }

    void EditorLayer::SaveActiveScene()
    {
        SceneManager* sceneManager = SceneManager::TryGet();
        if (sceneManager == nullptr || !CanSaveEditorScene(sceneManager->GetExecutionState()))
        {
            AddNotification("Stop Play or Simulate before saving the scene.");
            return;
        }

        const auto& scene = sceneManager->GetActiveScene();
        if (!scene)
            return;
        if (scene->GetFilepath().empty())
            SaveActiveSceneAs();
        else
        {
            scene->SetImGuiLayout(Application::TryGet()->GetImGuiLayer()->SaveLayout());
            SceneSerializer serializer(scene);
            serializer.Serialize(scene->GetFilepath());
            if (!SynchronizeActiveSceneAsset(scene))
                return;
            const String title = "Crowny Editor - " + Editor::Get().GetProjectName() + " - " + scene->GetName();
            Application::TryGet()->GetWindow().SetTitle(title);
            AddNotification(fmt::format("Saved {0}.", scene->GetFilepath().filename().string()), NotificationKind::Success);
        }
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

    void EditorLayer::AddRecentEntry(const Path& path)
    {
        // TODO: Don't write code like this...
        Ref<EditorSettings> settings = Editor::Get().GetEditorSettings();
        uint32_t recentIdx = settings->RecentProjects.size() > 0 ? (uint32_t)(settings->RecentProjects.size() - 1) : 0;
        for (uint32_t i = 0; i < settings->RecentProjects.size(); i++)
        {
            if (settings->RecentProjects[i].ProjectPath == Editor::Get().GetProjectPath())
                recentIdx = i;
        }
        for (uint32_t i = recentIdx; i >= 1; i--)
            settings->RecentProjects[i] = settings->RecentProjects[i - 1];

        settings->RecentProjects[0].Timestamp = std::time(nullptr);
        settings->RecentProjects[0].ProjectPath = path;
    }

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
