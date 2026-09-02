#include "cwepch.h"

#include "Editor/EditorLayer.h"

#include "Crowny/Application/EngineRuntime.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Import/Importer.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/PlatformUtils.h"
#include "Crowny/Events/ImGuiEvent.h"
#include "Crowny/ImGui/ImGuiMenu.h"
#include "Crowny/Input/Input.h"
#include "Crowny/Physics/Physics2D.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/EnvironmentMap.h"
#include "Crowny/Scene/Prefab.h"
#include "Crowny/Scene/SceneRenderer.h"
#include "Crowny/Scene/ScriptRuntime.h"
#include "Crowny/Scripting/Managed/ManagedScripting.h"
#include "Crowny/Scripting/ManagedReload.h"
#include "Crowny/Serialization/SceneSerializer.h"

#include "Editor/PrefabUtils.h"

#include "Panels/AssetBrowserPanel.h"
#include "Panels/AudioMixerPanel.h"
#include "Panels/EntityInspector.h"
#include "Panels/ConsolePanel.h"
#include "Panels/EditorPanelRegistry.h"
#include "Panels/HierarchyPanel.h"
#include "Panels/InputSettingsEditor.h"
#include "Panels/InspectorPanel.h"
#include "Panels/ViewportPanel.h"
#ifdef CW_WITH_NODES
#include "Panels/NodeEditor/NodeEditorPanel.h"
#endif

#include "Crowny/NodeGraph/BuiltinNodeTypes.h"

#include "Editor/ColliderOverlay.h"
#include "Editor/Editor.h"
#include "Editor/EditorAssets.h"
#include "Editor/ProjectLibrary.h"
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
#include <spdlog/fmt/fmt.h>

#include "Crowny/ImGui/ImGuiVulkanTexture.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

namespace Crowny
{
    void EditorLayer::OnImGuiRender()
    {
        // When no project is loaded, show the project hub and skip the editor UI
        if (!Editor::Get().IsProjectLoaded())
        {
            UI_ProjectManager();
            return;
        }

        SetupImGuiRender();

        m_MenuBar->Render();
        if (m_ShowDemoWindow)
            ImGui::ShowDemoWindow(&m_ShowDemoWindow);

        // UI_ProjectManager handles menu-triggered open/new even when a project is loaded
        UI_ProjectManager();
        UI_Header();
        UI_Settings();
        UI_BuildGame();
        UI_CommandPalette();

#ifdef CW_DEBUG
        UI_ScriptInfo();
        UI_AssetInfo();
        UI_EntityDebugInfo();
        Physics2D::TryGet()->UIStats();
#endif

        m_ViewportPanel->SetEditorRenderTarget(m_RenderTarget);
        m_ViewportPanel->SetShowStatistics(m_ShowRenderingStatistics);
        m_Panels->Render();
        UI_ViewportSettings();

        ImGui::End(); // End dockspace

        const bool importing = ProjectLibrary::IsStartedUp() && ProjectLibrary::Get().IsImporting();
        if (m_WasImporting && !importing)
            AddNotification("Asset import complete.", NotificationKind::Success);
        m_WasImporting = importing;

        // Status bar at the very bottom — rendered OUTSIDE the dockspace
        if (importing)
        {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            const float statusBarHeight = ImGui::GetFrameHeight();
            ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - statusBarHeight));
            ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, statusBarHeight));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 2.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::Begin("##StatusBar", nullptr,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoFocusOnAppearing |
                           ImGuiWindowFlags_NoNav);

            const ImportProgress progress = ProjectLibrary::Get().GetImportProgress();
            char text[128];
            snprintf(text, sizeof(text), "Importing assets... %u / %u", progress.CompletedFiles, progress.TotalFiles);
            ImGui::Text("%s", text);
            ImGui::SameLine();
            ImGui::ProgressBar(progress.GetFraction(), ImVec2(200, ImGui::GetFrameHeight() - 4));

            ImGui::End();
            ImGui::PopStyleVar(3);
        }

        UI_Notifications();
    }

    void EditorLayer::UI_ProjectManager()
    {
        // Handle menu-item flags when a project is already loaded:
        // Use native dialogs directly rather than showing the hub.
        if (Editor::Get().IsProjectLoaded())
        {
            if (m_OpenProject)
            {
                m_OpenProject = false;
                Vector<Path> outPaths;
                if (FileSystem::OpenFileDialog(FileDialogType::OpenFolder, outPaths, "Open Project", Editor::Get().GetDefaultProjectPath()))
                {
                    if (outPaths.size() > 0)
                    {
                        SaveProjectSettings();
                        m_AssetBrowser->Unload();
                        Editor::Get().LoadProject(outPaths[0]);
                        Editor::Get().GetEditorSettings()->LastOpenProject = outPaths[0];
                        SetProjectSettings();
                        m_AssetBrowser->Initialize();
                    }
                }
            }
            if (m_NewProject)
            {
                m_NewProject = false;
                SaveProjectSettings();
                m_AssetBrowser->Unload();
                Editor::Get().UnloadProject();
                m_HubPage = HubPage::NewProject;
                m_NewProjectPath = Editor::Get().GetDefaultProjectPath().string();
                m_NewProjectName = "New Project";
                // Will render hub on the next frame since project is now unloaded
            }
            return;
        }

        // Below: no project is loaded -- render the fullscreen hub

        // Fullscreen hub window covering the entire viewport
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGuiWindowFlags hubFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoDocking |
                                    ImGuiWindowFlags_NoSavedSettings;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("##ProjectHub", nullptr, hubFlags);
        ImGui::PopStyleVar();

        const ImVec2 windowSize = ImGui::GetContentRegionAvail();
        const float sidebarWidth = std::clamp(windowSize.x * 0.22f, 168.0f, 220.0f);

        // ---- Left sidebar ----
        {
            ImGui::BeginChild("##HubSidebar", ImVec2(sidebarWidth, windowSize.y), true);

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));

            // Title
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 16.0f);
            ImGui::SetCursorPosX(16.0f);
            ImGui::TextUnformatted("CROWNY");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Navigation buttons
            const float buttonWidth = sidebarWidth - ImGui::GetStyle().WindowPadding.x * 2.0f;
            bool isRecent = (m_HubPage == HubPage::RecentProjects);
            if (isRecent)
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button("Projects", ImVec2(buttonWidth, 0)))
                m_HubPage = HubPage::RecentProjects;
            if (isRecent)
                ImGui::PopStyleColor();

            bool isNew = (m_HubPage == HubPage::NewProject);
            if (isNew)
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button("New project", ImVec2(buttonWidth, 0)))
            {
                m_HubPage = HubPage::NewProject;
                if (m_NewProjectPath.empty())
                {
                    m_NewProjectPath = Editor::Get().GetDefaultProjectPath().string();
                    m_NewProjectName = "New Project";
                }
            }
            if (isNew)
                ImGui::PopStyleColor();

            ImGui::Separator();

            // Version info pushed to bottom
            ImGui::SetCursorPosY(ImGui::GetWindowHeight() - ImGui::GetTextLineHeightWithSpacing() - ImGui::GetStyle().WindowPadding.y);
            ImGui::SetCursorPosX(16.0f);
            ImGui::TextDisabled("v0.1.0-dev");

            ImGui::PopStyleVar();
            ImGui::EndChild();
        }

        ImGui::SameLine();

        // ---- Right content area ----
        {
            ImGui::BeginChild("##HubContent", ImVec2(0, windowSize.y), false);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
            const float contentPadding = 16.0f;
            ImGui::SetCursorPos(ImVec2(contentPadding, contentPadding));

            if (m_HubPage == HubPage::RecentProjects)
            {
                ImGui::TextUnformatted("Projects");
                ImGui::Spacing();

                const auto openProject = [this](const Path& projectPath) {
                    if (!fs::is_directory(projectPath))
                        return;
                    Editor::Get().LoadProject(projectPath);
                    Editor::Get().GetEditorSettings()->LastOpenProject = projectPath;
                    SetProjectSettings();
                    m_AssetBrowser->Initialize();
                };
                const auto browseForProject = [this, &openProject]() {
                    Vector<Path> outPaths;
                    if (FileSystem::OpenFileDialog(FileDialogType::OpenFolder, outPaths, "Open Project", Editor::Get().GetDefaultProjectPath()) &&
                        !outPaths.empty())
                        openProject(outPaths.front());
                };

                // Lay the toolbar out with explicit widths instead of a table: a freshly created table needs a frame or
                // two of auto-fit before its stretch column settles, which made the search field pop in at a wrong size.
                const float toolbarWidth = ImGui::GetContentRegionAvail().x - contentPadding;
                const float openButtonWidth = 120.0f;
                const float toolbarSpacing = ImGui::GetStyle().ItemSpacing.x;
                if (toolbarWidth >= 390.0f)
                {
                    ImGui::SetNextItemWidth(toolbarWidth - openButtonWidth - toolbarSpacing);
                    UIUtils::SearchWidget(m_RecentSearchFilter, "Search projects...");
                    ImGui::SameLine(0.0f, toolbarSpacing);
                    if (ImGui::Button("Open project...", ImVec2(openButtonWidth, 0.0f)))
                        browseForProject();
                    UI::SetTooltip("Choose a project folder");
                }
                else
                {
                    ImGui::SetNextItemWidth(toolbarWidth);
                    UIUtils::SearchWidget(m_RecentSearchFilter, "Search projects...");
                    if (ImGui::Button("Open project...", ImVec2(toolbarWidth, 0.0f)))
                        browseForProject();
                    UI::SetTooltip("Choose a project folder");
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                Ref<EditorSettings> settings = Editor::Get().GetEditorSettings();
                uint32_t recentCount = 0;
                uint32_t matchingCount = 0;
                bool selectedVisible = false;
                String normalizedFilter = m_RecentSearchFilter;
                std::transform(normalizedFilter.begin(), normalizedFilter.end(), normalizedFilter.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

                ImGui::BeginChild("##RecentList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2.0f - 10.0f), false);
                for (uint32_t i = 0; i < settings->RecentProjects.size(); i++)
                {
                    const RecentProject& project = settings->RecentProjects[i];
                    if (project.ProjectPath.empty())
                        continue;
                    recentCount++;

                    const String projectName = project.ProjectPath.filename().string();
                    String searchable = projectName + " " + project.ProjectPath.string();
                    std::transform(searchable.begin(), searchable.end(), searchable.begin(),
                                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    if (!normalizedFilter.empty() && searchable.find(normalizedFilter) == String::npos)
                        continue;
                    matchingCount++;
                    if (m_SelectedRecentIdx == static_cast<int>(i))
                        selectedVisible = true;

                    char timeStr[32] = "Never opened";
                    if (project.Timestamp != 0)
                    {
                        tm timeinfo{};
#ifdef CW_PLATFORM_WIN32
                        localtime_s(&timeinfo, &project.Timestamp);
#else
                        localtime_r(&project.Timestamp, &timeinfo);
#endif
                        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", &timeinfo);
                    }

                    ImGui::PushID(static_cast<int>(i));
                    const bool isSelected = (m_SelectedRecentIdx == static_cast<int>(i));
                    const bool pathExists = fs::is_directory(project.ProjectPath);
                    const float itemHeight = ImGui::GetTextLineHeightWithSpacing() * 2.0f + 12.0f;

                    if (ImGui::Selectable("##recentEntry", isSelected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0, itemHeight)))
                    {
                        m_SelectedRecentIdx = static_cast<int>(i);
                        if (pathExists && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                            openProject(project.ProjectPath);
                    }

                    const ImVec2 itemMin = ImGui::GetItemRectMin();
                    const ImVec2 cursorAfterItem = ImGui::GetCursorPos();
                    ImGui::SetCursorScreenPos(ImVec2(itemMin.x + 10.0f, itemMin.y + 5.0f));
                    ImGui::TextUnformatted(projectName.c_str());
                    ImGui::SetCursorScreenPos(ImVec2(itemMin.x + 10.0f, itemMin.y + 5.0f + ImGui::GetTextLineHeightWithSpacing()));
                    if (pathExists)
                        ImGui::TextDisabled("%s  |  %s", project.ProjectPath.string().c_str(), timeStr);
                    else
                        ImGui::TextColored(ImVec4(0.95f, 0.48f, 0.38f, 1.0f), "Folder missing  |  %s", project.ProjectPath.string().c_str());
                    UI::SetTooltip(project.ProjectPath.string());
                    // The overlay text moved the cursor; restore it so the next entry starts below this one.
                    ImGui::SetCursorPos(cursorAfterItem);

                    ImGui::PopID();
                }

                if (matchingCount == 0)
                {
                    const char* emptyMessage = recentCount == 0 ? "No recent projects." : "No projects match this search.";
                    const float emptyWidth = ImGui::CalcTextSize(emptyMessage).x;
                    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), (ImGui::GetContentRegionAvail().x - emptyWidth) * 0.5f));
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 28.0f);
                    ImGui::TextDisabled("%s", emptyMessage);
                    if (recentCount == 0)
                        ImGui::TextDisabled("Open a project folder or create a new project.");
                    else if (ImGui::Button("Clear search"))
                        m_RecentSearchFilter.clear();
                }
                ImGui::EndChild();

                ImGui::Separator();
                ImGui::Spacing();

                const bool hasSelection = m_SelectedRecentIdx >= 0 && m_SelectedRecentIdx < static_cast<int>(settings->RecentProjects.size()) &&
                                          !settings->RecentProjects[m_SelectedRecentIdx].ProjectPath.empty() && selectedVisible;
                const bool selectedPathExists = hasSelection && fs::is_directory(settings->RecentProjects[m_SelectedRecentIdx].ProjectPath);
                ImGui::TextDisabled("%u project%s", matchingCount, matchingCount == 1 ? "" : "s");
                ImGui::SameLine();

                if (!selectedPathExists)
                    ImGui::BeginDisabled();
                if (ImGui::Button("Open", ImVec2(72.0f, 0)))
                {
                    const RecentProject& sel = settings->RecentProjects[m_SelectedRecentIdx];
                    openProject(sel.ProjectPath);
                }
                if (!selectedPathExists)
                    ImGui::EndDisabled();
                ImGui::SameLine();

                if (!hasSelection)
                    ImGui::BeginDisabled();
                if (ImGui::Button("Remove from list"))
                {
                    for (uint32_t j = m_SelectedRecentIdx; j < settings->RecentProjects.size() - 1; j++)
                        settings->RecentProjects[j] = settings->RecentProjects[j + 1];
                    settings->RecentProjects[settings->RecentProjects.size() - 1].ProjectPath.clear();
                    settings->RecentProjects[settings->RecentProjects.size() - 1].Timestamp = 0;
                    m_SelectedRecentIdx = -1;
                }
                UI::SetTooltip("Removes this shortcut. Project files stay on disk.");
                ImGui::SameLine();
                if (!selectedPathExists)
                    ImGui::BeginDisabled();
                if (ImGui::Button("Show in folder"))
                    PlatformUtils::ShowInExplorer(settings->RecentProjects[m_SelectedRecentIdx].ProjectPath);
                UI::SetTooltip(selectedPathExists ? "Open the project folder" : "Project folder not found");
                if (!selectedPathExists)
                    ImGui::EndDisabled();
                if (!hasSelection)
                    ImGui::EndDisabled();
            }
            else if (m_HubPage == HubPage::NewProject)
            {
                ImGui::TextUnformatted("Create a project");
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextUnformatted("Name");
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - contentPadding);
                ImGui::InputText("##newProjectName", &m_NewProjectName);

                ImGui::Spacing();
                ImGui::TextUnformatted("Location");
                const auto chooseProjectLocation = [this]() {
                    Vector<Path> outPaths;
                    if (FileSystem::OpenFileDialog(FileDialogType::OpenFolder, outPaths, "Select Location", Path(m_NewProjectPath)) &&
                        !outPaths.empty())
                        m_NewProjectPath = outPaths.front().string();
                };
                const float locationWidth = ImGui::GetContentRegionAvail().x - contentPadding;
                if (locationWidth >= 340.0f &&
                    ImGui::BeginTable("##projectLocation", 2,
                                      ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_NoPadOuterX))
                {
                    ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Browse", ImGuiTableColumnFlags_WidthFixed, 82.0f);
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::InputText("##newProjectPath", &m_NewProjectPath);
                    ImGui::TableSetColumnIndex(1);
                    if (ImGui::Button("Browse...", ImVec2(-1.0f, 0.0f)))
                        chooseProjectLocation();
                    ImGui::EndTable();
                }
                else if (locationWidth < 340.0f)
                {
                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::InputText("##newProjectPath", &m_NewProjectPath);
                    if (ImGui::Button("Browse...", ImVec2(-1.0f, 0.0f)))
                        chooseProjectLocation();
                }

                ImGui::Spacing();
                ImGui::Spacing();

                // Real-time validation
                const bool pathExists = fs::exists(m_NewProjectPath);
                const bool projectExists = pathExists && fs::exists(Path(m_NewProjectPath) / m_NewProjectName);
                const bool nameEmpty = m_NewProjectName.empty();
                const bool canCreate = pathExists && !projectExists && !nameEmpty;

                if (nameEmpty)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                    ImGui::TextUnformatted("Project name cannot be empty");
                    ImGui::PopStyleColor();
                }
                else if (!pathExists)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                    ImGui::TextUnformatted("Path does not exist");
                    ImGui::PopStyleColor();
                }
                else if (projectExists)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                    ImGui::TextUnformatted("A project with this name already exists");
                    ImGui::PopStyleColor();
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
                    ImGui::TextUnformatted("Ready to create");
                    ImGui::PopStyleColor();
                }

                ImGui::Spacing();

                if (!canCreate)
                    ImGui::BeginDisabled();

                if (ImGui::Button("Create project", ImVec2(120.0f, 0)))
                {
                    Editor::Get().CreateProject(m_NewProjectPath, m_NewProjectName);
                    Path newProjectPath = Path(m_NewProjectPath) / m_NewProjectName;
                    Editor::Get().LoadProject(newProjectPath);
                    Editor::Get().GetEditorSettings()->LastOpenProject = newProjectPath;
                    SetProjectSettings();
                    m_NewProjectPath.clear();
                    m_NewProjectName.clear();
                    m_AssetBrowser->Initialize();
                }

                if (!canCreate)
                    ImGui::EndDisabled();
            }

            ImGui::PopStyleVar();
            ImGui::EndChild();
        }

        ImGui::End();
    }

    void EditorLayer::UI_EntityDebugInfo()
    {
        if (m_ShowEntityDebugInfo)
        {
            ImGui::Begin("Entity Debug Info", &m_ShowEntityDebugInfo);
            const Ref<Scene> scene = SceneManager::TryGet()->GetActiveScene();
            auto view = scene->GetAllEntitiesWith<TagComponent>();
            for (auto e : view)
            {
                Entity entity = Entity(e, scene.get());
                const String label = entity.GetName() + ": " + entity.GetUuid().ToString();
                if (ImGui::TreeNode(label.c_str()))
                {
                    if (entity.GetParent())
                    {
                        const String parentLabel = entity.GetParent().GetName() + ": " + entity.GetParent().GetUuid().ToString();
                        ImGui::Text("%s", parentLabel.c_str());
                    }
                    ImGui::TreePop();
                }
            }
            ImGui::End();
        }
    }

    void EditorLayer::UI_AssetInfo()
    {
        if (m_ShowAssetInfo)
        {
            ImGui::Begin("Asset Info", &m_ShowAssetInfo);
            ImGui::Columns(3);
            ImGui::Text("Project Library");
            ImGui::NextColumn();
            ImGui::Text("Show empty metadata entries");
            ImGui::SameLine();
            ImGui::Checkbox("##showEmptyMetadata", &m_ShowEmptyMetadataAssetInfo);
            ImGui::NextColumn();
            ImGui::NextColumn();

            static const Map<AssetType, const char*> assetTypes = {
                { AssetType::None, "None" },
                { AssetType::AudioClip, "Audio Clip" },
                { AssetType::Material, "Material" },
                { AssetType::Mesh, "Mesh" },
                { AssetType::MeshSource, "Mesh Source" },
                { AssetType::PhysicsMaterial, "Physics Material" },
                { AssetType::PhysicsMaterial2D, "Physics Material 2D" },
                { AssetType::PhysicsMesh, "Physics Mesh" },
                { AssetType::PlainText, "Plain Text" },
                { AssetType::ScriptCode, "Script Code" },
                { AssetType::Shader, "Shader" },
                { AssetType::Texture, "Texture" },
                { AssetType::Font, "Font" },
                { AssetType::Scene, "Scene" },
            };

            std::function<void(const Ref<LibraryEntry>&)> traverse = [&](const Ref<LibraryEntry>& entry) {
                if (entry->Type == LibraryEntryType::Directory)
                {
                    for (auto& child : StaticRefCast<DirectoryEntry>(entry)->Children)
                        traverse(child);
                }
                else
                {
                    FileEntry* file = static_cast<FileEntry*>(entry.get());

                    if (!m_ShowEmptyMetadataAssetInfo && file->Metadata == nullptr)
                        return;
                    ImGui::Text("%s", file->Filepath.string().c_str());
                    ImGui::NextColumn();
                    if (file->Metadata != nullptr)
                        ImGui::Text("%s", file->Metadata->Uuid.ToString().c_str());
                    ImGui::NextColumn();
                    if (file->Metadata != nullptr && assetTypes.count(file->Metadata->Type))
                        ImGui::Text("%s", assetTypes.at(file->Metadata->Type));
                    ImGui::NextColumn();
                }
            };
            const Ref<DirectoryEntry>& root = ProjectLibrary::Get().GetRoot();
            traverse(root);
            ImGui::End();
        }
    }

    void EditorLayer::UI_Physics2DSettings()
    {
        UI::BeginPropertyGrid();
        glm::vec2 gravity = Physics2D::TryGet()->GetGravity();
        if (UI::Property("Gravity", gravity))
            Physics2D::TryGet()->SetGravity(gravity);

        uint32_t velocityIterations = Physics2D::TryGet()->GetVelocityIterations();
        if (UI::Property("Velocity iterations", velocityIterations))
            Physics2D::TryGet()->SetVelocityIterations(std::max(1u, velocityIterations));

        uint32_t positionIterations = Physics2D::TryGet()->GetPositionIterations();
        if (UI::Property("Position iterations", positionIterations))
            Physics2D::TryGet()->SetPositionIterations(std::max(1u, positionIterations));
        UI::EndPropertyGrid();

        ImGui::Spacing();
        if (ImGui::TreeNodeEx("Layer names", ImGuiTreeNodeFlags_DefaultOpen))
        {
            uint32_t lastVisibleLayer = 0;
            for (uint32_t i = 0; i < 32; i++)
            {
                if (!Physics2D::TryGet()->GetLayerName(i).empty())
                    lastVisibleLayer = i;
            }
            lastVisibleLayer = std::min(31u, lastVisibleLayer + 1u);

            if (ImGui::BeginTable("##PhysicsLayers", 2,
                                  ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_RowBg |
                                    ImGuiTableFlags_BordersInnerH))
            {
                ImGui::TableSetupColumn("Layer", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                for (uint32_t i = 0; i <= lastVisibleLayer; i++)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("Layer %u", i);
                    ImGui::TableSetColumnIndex(1);
                    String layerName = Physics2D::TryGet()->GetLayerName(i);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::PushID(i);
                    if (ImGui::InputTextWithHint("##LayerName", i == 0 ? "Default" : "Unused", &layerName))
                        Physics2D::TryGet()->SetLayerName(i, layerName);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }

        ImGui::Spacing();
        if (ImGui::TreeNode("Collision matrix"))
        {
            Vector<uint32_t> namedLayers;
            for (uint32_t i = 0; i < 32; i++)
            {
                if (!Physics2D::TryGet()->GetLayerName(i).empty())
                    namedLayers.push_back(i);
            }

            if (namedLayers.empty())
                ImGui::TextDisabled("Name a layer to edit collision rules.");
            else
            {
                ImGui::TextDisabled("Rows define which named layers each layer collides with.");
                ImGui::BeginChild("##CollisionMatrixScroll", ImVec2(0.0f, std::min(260.0f, 54.0f + namedLayers.size() * 28.0f)), true,
                                  ImGuiWindowFlags_HorizontalScrollbar);
                if (ImGui::BeginTable("##CollisionMatrix", (int32_t)namedLayers.size() + 1,
                                      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX | ImGuiTableFlags_SizingFixedFit |
                                        ImGuiTableFlags_NoSavedSettings))
                {
                    ImGui::TableSetupColumn("Layer", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                    for (uint32_t layer : namedLayers)
                        ImGui::TableSetupColumn(Physics2D::TryGet()->GetLayerName(layer).c_str(), ImGuiTableColumnFlags_WidthFixed, 76.0f);
                    ImGui::TableHeadersRow();

                    for (uint32_t rowLayer : namedLayers)
                    {
                        uint32_t categoryMask = Physics2D::TryGet()->GetCategoryMask(rowLayer);
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextUnformatted(Physics2D::TryGet()->GetLayerName(rowLayer).c_str());
                        for (uint32_t column = 0; column < namedLayers.size(); column++)
                        {
                            const uint32_t columnLayer = namedLayers[column];
                            ImGui::TableSetColumnIndex((int32_t)column + 1);
                            bool collides = (categoryMask & (1u << columnLayer)) != 0;
                            ImGui::PushID((int32_t)(rowLayer * 32u + columnLayer));
                            if (ImGui::Checkbox("##Collides", &collides))
                            {
                                categoryMask = collides ? categoryMask | (1u << columnLayer) : categoryMask & ~(1u << columnLayer);
                                Physics2D::TryGet()->SetCategoryMask(rowLayer, categoryMask);
                            }
                            ImGui::PopID();
                        }
                    }
                    ImGui::EndTable();
                }
                ImGui::EndChild();
            }
            ImGui::TreePop();
        }
    }

    void EditorLayer::UI_TimeSettings()
    {
        const Ref<TimeSettings>& timeSettings = Application::TryGet()->GetTimeSettings();
        ImGui::PushID("TimeSettings"); // Keep this grid's generated ids apart from the other grids in the Settings window.
        UI::BeginPropertyGrid();
        if (UI::Property("Time scale", timeSettings->TimeScale))
            timeSettings->TimeScale = std::max(0.0f, timeSettings->TimeScale);
        if (UI::Property("Fixed timestep", timeSettings->FixedTimestep))
            timeSettings->FixedTimestep = std::max(0.0001f, timeSettings->FixedTimestep);
        if (UI::Property("Maximum timestep", timeSettings->MaxTimestep))
            timeSettings->MaxTimestep = std::max(timeSettings->FixedTimestep, timeSettings->MaxTimestep);
        UI::EndPropertyGrid();
        ImGui::PopID();
    }

    static void DrawScriptType(const ScriptTypeSchema& type)
    {
        const String label = type.Identity.Assembly + ":" + type.Identity.GetFullName();
        if (ImGui::TreeNode(label.c_str()))
        {
            ImGui::Text("Stable id: %llu", static_cast<unsigned long long>(type.StableId));
            ImGui::Text("Runs in editor: %s", (type.Flags & ScriptTypeFlags::RunInEditor) != ScriptTypeFlags::None ? "yes" : "no");
            if (ImGui::TreeNode("Callbacks"))
            {
                for (ScriptEventKind event : type.Events)
                    ImGui::BulletText("Event %u", static_cast<uint32_t>(event));
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Fields"))
            {
                for (const ScriptFieldSchema& field : type.Fields)
                {
                    ImGui::BulletText("%s (kind %u, stable id %llu)", field.Name.c_str(), static_cast<uint32_t>(field.ValueKind),
                                      static_cast<unsigned long long>(field.StableId));
                }
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }
    }

    void EditorLayer::AddNotification(const String& message, NotificationKind kind)
    {
        // Errors stay a little longer than informational cards; a card may also be closed with its X button.
        const float lifetime = kind == NotificationKind::Error ? 8.0f : 5.0f;
        for (Notification& notification : m_Notifications)
        {
            if (notification.Message == message)
            {
                notification.Kind = kind;
                notification.SecondsLeft = lifetime;
                return;
            }
        }

        if (m_Notifications.size() >= 4)
            m_Notifications.erase(m_Notifications.begin());
        m_Notifications.push_back({ m_NextNotificationId++, message, kind, lifetime });
    }

    void EditorLayer::UI_Notifications()
    {
        if (m_Notifications.empty())
            return;

        constexpr float fadeDuration = 0.4f;
        constexpr float maxCardWidth = 380.0f;
        const float deltaTime = std::max(0.0f, ImGui::GetIO().DeltaTime);
        const ImGuiStyle& style = ImGui::GetStyle();

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        float y = viewport->WorkPos.y + 42.0f;
        for (size_t i = 0; i < m_Notifications.size();)
        {
            Notification& notification = m_Notifications[i];
            const ImVec4 accent = notification.Kind == NotificationKind::Success ? ImVec4(0.30f, 0.78f, 0.44f, 1.0f)
                                  : notification.Kind == NotificationKind::Error ? ImVec4(0.92f, 0.30f, 0.28f, 1.0f)
                                                                                 : ImGui::ColorConvertU32ToFloat4(UI::Colors::Accent);
            const float alpha = std::clamp(notification.SecondsLeft / fadeDuration, 0.0f, 1.0f);

            ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 14.0f, y), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
            ImGui::SetNextWindowSizeConstraints(ImVec2(250.0f, 0.0f), ImVec2(maxCardWidth, FLT_MAX));
            ImGui::SetNextWindowBgAlpha(0.96f * alpha);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
            ImGui::PushStyleColor(ImGuiCol_Border, accent);
            const String windowName = fmt::format("##Notification{0}", notification.Id);
            ImGui::Begin(windowName.c_str(), nullptr,
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove |
                           ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings);

            // Reserve the close button's column so the wrapped message never runs underneath it.
            const float closeSize = ImGui::GetFrameHeight();
            const float wrapWidth = maxCardWidth - style.WindowPadding.x * 2.0f - closeSize - style.ItemSpacing.x;
            ImGui::BeginGroup();
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrapWidth);
            ImGui::TextUnformatted(notification.Message.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndGroup();
            ImGui::SameLine();
            const bool dismissed = ImGui::CloseButton(ImGui::GetID("##close"), ImGui::GetCursorScreenPos());
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Dismiss");

            // Hovering pauses the countdown (and cancels an in-progress fade) so the user can finish reading.
            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
                notification.SecondsLeft = std::max(notification.SecondsLeft, fadeDuration);
            else
                notification.SecondsLeft -= deltaTime;

            y += ImGui::GetWindowSize().y + 8.0f;
            ImGui::End();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);

            if (dismissed || notification.SecondsLeft <= 0.0f)
                m_Notifications.erase(m_Notifications.begin() + static_cast<ptrdiff_t>(i));
            else
                i++;
        }
    }

    void EditorLayer::ResetWorkspaceLayout()
    {
        m_ResetLayoutRequested = true;
        AddNotification("Default workspace layout restored.", NotificationKind::Success);
    }

    void EditorLayer::TogglePlay()
    {
        if (m_SceneState == SceneState::Edit)
        {
            if (!SceneManager::TryGet()->GetActiveScene())
            {
                AddNotification("Open a scene before starting play mode.", NotificationKind::Error);
                return;
            }
            const Entity selected = m_HierarchyPanel->GetSelectedEntity();
            const UUID selectedId = selected ? selected.GetUuid() : UUID::EMPTY;
            const SceneOperationStatus result = SceneManager::TryGet()->BeginPlay(selectedId);
            AddNotification(result == SceneOperationStatus::Completed || result == SceneOperationStatus::Deferred ? "Play mode started."
                                                                                                                  : "Could not start play mode.",
                            result == SceneOperationStatus::Completed || result == SceneOperationStatus::Deferred ? NotificationKind::Info
                                                                                                                  : NotificationKind::Error);
        }
        else if (m_SceneState == SceneState::Play || m_SceneState == SceneState::PausePlay)
        {
            const SceneOperationStatus result = SceneManager::TryGet()->Stop();
            AddNotification(result == SceneOperationStatus::Completed || result == SceneOperationStatus::Deferred ? "Play mode stopped."
                                                                                                                  : "Could not stop play mode.",
                            result == SceneOperationStatus::Completed || result == SceneOperationStatus::Deferred ? NotificationKind::Info
                                                                                                                  : NotificationKind::Error);
        }
    }

    void EditorLayer::ToggleSimulation()
    {
        if (m_SceneState == SceneState::Edit)
        {
            if (!SceneManager::TryGet()->GetActiveScene())
            {
                AddNotification("Open a scene before starting physics simulation.", NotificationKind::Error);
                return;
            }
            const Entity selected = m_HierarchyPanel->GetSelectedEntity();
            const UUID selectedId = selected ? selected.GetUuid() : UUID::EMPTY;
            const SceneOperationStatus result = SceneManager::TryGet()->BeginSimulation(selectedId);
            AddNotification(result == SceneOperationStatus::Completed || result == SceneOperationStatus::Deferred
                              ? "Physics simulation started."
                              : "Could not start physics simulation.",
                            result == SceneOperationStatus::Completed || result == SceneOperationStatus::Deferred ? NotificationKind::Info
                                                                                                                  : NotificationKind::Error);
        }
        else if (m_SceneState == SceneState::Simulate)
        {
            const SceneOperationStatus result = SceneManager::TryGet()->Stop();
            AddNotification(result == SceneOperationStatus::Completed || result == SceneOperationStatus::Deferred
                              ? "Physics simulation stopped."
                              : "Could not stop physics simulation.",
                            result == SceneOperationStatus::Completed || result == SceneOperationStatus::Deferred ? NotificationKind::Info
                                                                                                                  : NotificationKind::Error);
        }
    }

    void EditorLayer::TogglePause()
    {
        if (m_SceneState == SceneState::Play)
        {
            const SceneOperationStatus result = SceneManager::TryGet()->PausePlay();
            AddNotification(result == SceneOperationStatus::Completed || result == SceneOperationStatus::Deferred ? "Play mode paused."
                                                                                                                  : "Could not pause play mode.",
                            result == SceneOperationStatus::Completed || result == SceneOperationStatus::Deferred ? NotificationKind::Info
                                                                                                                  : NotificationKind::Error);
        }
        else if (m_SceneState == SceneState::PausePlay)
        {
            const SceneOperationStatus result = SceneManager::TryGet()->ResumePlay();
            AddNotification(result == SceneOperationStatus::Completed || result == SceneOperationStatus::Deferred ? "Play mode resumed."
                                                                                                                  : "Could not resume play mode.",
                            result == SceneOperationStatus::Completed || result == SceneOperationStatus::Deferred ? NotificationKind::Info
                                                                                                                  : NotificationKind::Error);
        }
    }

    void EditorLayer::UI_CommandPalette()
    {
        if (!m_ShowCommandPalette)
            return;

        struct Command
        {
            const char* Name;
            const char* Shortcut;
            std::function<void()> Execute;
        };

        const Vector<Command> commands = {
            { "Save scene", "Ctrl+S", [this]() { SaveActiveScene(); } },
            { "Save scene as", "Ctrl+Shift+S", [this]() { SaveActiveSceneAs(); } },
            { "Save project", "",
              [this]() {
                  Editor::Get().SaveProject();
                  AddNotification("Project saved.", NotificationKind::Success);
              } },
            { "New scene", "Ctrl+Shift+N", [this]() { CreateNewScene(); } },
            { "Open scene", "Ctrl+Shift+O", [this]() { OpenScene(); } },
            { "Build game", "Ctrl+B", [this]() { BuildGame(); } },
            { "Rebuild game assembly", "Ctrl+Shift+B",
              [this]() {
                  const bool built = RebuildAssemblies();
                  AddNotification(built ? "Game scripts rebuilt." : "Game script build failed.",
                                  built ? NotificationKind::Success : NotificationKind::Error);
              } },
            { "Open settings", "Ctrl+,", [this]() { m_ShowSettings = true; } },
            { "Open viewport settings", "", [this]() { m_ShowViewportSettings = true; } },
            { "Reset workspace layout", "", [this]() { ResetWorkspaceLayout(); } },
            { "Show asset browser", "", [this]() { m_AssetBrowser->Show(); } },
            { "Show console", "", [this]() { m_ConsolePanel->Show(); } },
            { "Show inspector", "", [this]() { m_InspectorPanel->Show(); } },
            { "Toggle grid", "", [this]() { m_ShowGrid = !m_ShowGrid; } },
            { "Toggle collider outlines", "", [this]() { m_ShowColliders = !m_ShowColliders; } },
            { m_SceneState == SceneState::Edit ? "Start play mode" : "Stop play mode", "F5", [this]() { TogglePlay(); } },
            { m_SceneState == SceneState::Simulate ? "Stop physics simulation" : "Start physics simulation", "", [this]() { ToggleSimulation(); } },
        };

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.35f));
        ImGui::SetNextWindowSize(ImVec2(520.0f, 420.0f), ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints(ImVec2(380.0f, 240.0f), ImVec2(720.0f, 720.0f));

        std::function<void()> pendingCommand;
        if (ImGui::Begin("Command palette", &m_ShowCommandPalette,
                         ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
        {
            UIUtils::SearchWidget(m_CommandSearch, "Type a command...", &m_FocusCommandPalette);
            ImGui::Separator();

            const Command* firstMatch = nullptr;
            if (ImGui::BeginTable("##Commands", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
            {
                ImGui::TableSetupColumn("Command", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed, 110.0f);
                for (const Command& command : commands)
                {
                    if (!m_CommandSearch.empty() && !StringUtils::IsSearchMathing(command.Name, m_CommandSearch, false, true, true))
                        continue;
                    if (firstMatch == nullptr)
                        firstMatch = &command;

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    if (ImGui::Selectable(command.Name, false, ImGuiSelectableFlags_SpanAllColumns))
                        pendingCommand = command.Execute;
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextDisabled("%s", command.Shortcut);
                }
                ImGui::EndTable();
            }

            if (firstMatch == nullptr)
                ImGui::TextDisabled("No matching commands.");
            if (firstMatch != nullptr && ImGui::IsKeyPressed(ImGuiKey_Enter))
                pendingCommand = firstMatch->Execute;
            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                m_ShowCommandPalette = false;
        }
        ImGui::End();

        if (pendingCommand)
        {
            m_ShowCommandPalette = false;
            m_CommandSearch.clear();
            pendingCommand();
        }
    }

    void EditorLayer::UI_ScriptInfo()
    {
        if (m_ShowScriptDebugInfo)
        {
            ImGui::Begin("C# debug", &m_ShowScriptDebugInfo);
            static AssetHandle<AudioClip> audioHandle;
            UIUtils::AssetReference<AudioClip>("Clip", audioHandle);
            static AssetHandle<Shader> shaderHandle;
            UIUtils::AssetReference<Shader>("Shader", shaderHandle);
            ManagedScripting* managed = Application::TryGet()->GetRuntime().GetManagedScripting();
            if (managed == nullptr || !managed->IsStarted())
                ImGui::TextDisabled("Managed scripting is unavailable.");
            else
            {
                const ScriptCatalog& catalog = managed->GetScriptCatalog();
                ImGui::Text("Manifest %u, %zu script types", catalog.ManifestVersion, catalog.Types.size());
                for (const ScriptTypeSchema& type : catalog.Types)
                    DrawScriptType(type);
            }
            ImGui::End();
        }
    }

    void EditorLayer::UI_Header()
    {
        UI::PushID();

        UI::ScopedStyle disableSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        UI::ScopedStyle disableWindowBorder(ImGuiStyleVar_WindowBorderSize, 0.0f);
        UI::ScopedStyle windowRounding(ImGuiStyleVar_WindowRounding, 4.0f);
        UI::ScopedStyle disablePadding(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        const float buttonSize = 18.0f + 5.0f;
        const float edgeOffset = 4.0f;
        const float windowHeight = 32.0f;
        const float numberOfButtons = 3.0f;
        const float stateWidth = 72.0f;
        const float backgroundWidth = edgeOffset * 6.0f + buttonSize * numberOfButtons + edgeOffset * (numberOfButtons - 1.0f) * 2.0f + stateWidth;

        const float toolbarX = (m_ViewportPanel->GetViewportBounds().x + m_ViewportPanel->GetViewportBounds().z) / 2.0f;
        ImGui::SetNextWindowPos(ImVec2(toolbarX - (backgroundWidth / 2.0f), m_ViewportPanel->GetViewportBounds().y + edgeOffset));
        ImGui::SetNextWindowSize(ImVec2(backgroundWidth, windowHeight));
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::Begin("##viewport_central_toolbar", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking);

        const float desiredHeight = 26.0f + 5.0f;
        const ImRect background = UI::RectExpanded(ImGui::GetCurrentWindow()->Rect(), 0.0f, -(windowHeight - desiredHeight) * 0.5f);
        // Floating HUD background: bg_elevated #221E1A at 70% alpha (matches design spec).
        ImGui::GetWindowDrawList()->AddRectFilled(background.Min, background.Max, IM_COL32(34, 30, 26, 178), 3.0f);

        ImGui::BeginVertical("##viewport_central_toolbarV", { backgroundWidth, ImGui::GetContentRegionAvail().y });
        ImGui::Spring();
        ImGui::BeginHorizontal("##viewport_central_toolbarH", { backgroundWidth, ImGui::GetContentRegionAvail().y });
        ImGui::Spring();
        {
            UI::ScopedStyle enableSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(edgeOffset * 2.0f, 0));
            const ImColor c_ButtonTint = IM_COL32(138, 125, 114, 255); // text_secondary
            const bool isPlaying = m_SceneState == SceneState::Play || m_SceneState == SceneState::PausePlay;
            const ImColor c_PlayButtonTint = isPlaying ? ImColor(UI::Colors::Accent) : c_ButtonTint;
            const ImColor c_SimulateButtonTint = m_SceneState == SceneState::Simulate ? ImColor(UI::Colors::Accent) : c_ButtonTint;
            const ImColor c_PauseButtonTint = m_SceneState == SceneState::PausePlay ? ImColor(UI::Colors::Accent) : c_ButtonTint;

            auto drawButton = [buttonSize](const Ref<Texture>& icon, const ImColor& tint, float paddingY = 0.0f) {
                const float height = std::min((float)icon->GetHeight(), buttonSize) - paddingY * 2.0f;
                const float width = (float)icon->GetWidth() / (float)icon->GetHeight() * height;
                const bool clicked = ImGui::InvisibleButton(UI::GenerateID(), ImVec2(width, height));
                UI::DrawButtonImage(icon, tint, tint, tint, UI::RectOffset(UI::GetItemRect(), 0.0f, paddingY));

                return clicked;
            };

            const Ref<Texture> buttonTex = isPlaying ? EditorAssets::Get().StopIcon : EditorAssets::Get().PlayIcon;
            ImGui::BeginDisabled(m_SceneState == SceneState::Simulate);
            if (drawButton(buttonTex, c_PlayButtonTint))
                TogglePlay();
            UI::SetTooltip(isPlaying ? "Stop play mode (F5)" : "Start play mode (F5)");
            ImGui::EndDisabled();

            ImGui::BeginDisabled(isPlaying);
            if (drawButton(m_SceneState == SceneState::Simulate ? EditorAssets::Get().StopIcon : EditorAssets::Get().PlayIcon, c_SimulateButtonTint))
                ToggleSimulation();
            UI::SetTooltip(m_SceneState == SceneState::Simulate ? "Stop physics simulation" : "Start physics simulation");
            ImGui::EndDisabled();

            ImGui::BeginDisabled(!isPlaying);
            if (drawButton(EditorAssets::Get().PauseIcon, c_PauseButtonTint))
                TogglePause();
            UI::SetTooltip(m_SceneState == SceneState::PausePlay ? "Resume play mode" : "Pause play mode");
            ImGui::EndDisabled();

            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            const char* stateLabel = m_SceneState == SceneState::Play        ? "Playing"
                                     : m_SceneState == SceneState::PausePlay ? "Paused"
                                     : m_SceneState == SceneState::Simulate  ? "Simulating"
                                                                             : "Editing";
            ImGui::TextDisabled("%s", stateLabel);
        }
        ImGui::Spring();
        ImGui::EndHorizontal();
        ImGui::Spring();
        ImGui::EndVertical();

        ImGui::End();

        UI::PopID();
    }

    void EditorLayer::UI_ViewportSettings()
    {
        if (!m_ShowViewportSettings)
            return;

        UI::PushID();
        UI::ScopedStyle windowRounding(ImGuiStyleVar_WindowRounding, 6.0f);

        const float settingsWidth = 240.0f;
        const float edgeOffset = 4.0f;
        const float toolbarHeight = 32.0f;
        const float settingsX = m_ViewportPanel->GetViewportBounds().x + edgeOffset;
        const float settingsY = m_ViewportPanel->GetViewportBounds().y + edgeOffset + toolbarHeight + 4.0f;

        ImGui::SetNextWindowPos(ImVec2(settingsX, settingsY), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(settingsWidth, 0.0f)); // auto height
        ImGui::SetNextWindowBgAlpha(0.85f);
        ImGui::Begin("##viewport_settings_popup", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                       ImGuiWindowFlags_NoNav);
        m_ViewportPanel->SetViewportSettingsHovered(ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows));

        ImGui::Spacing();

        // ── Rendering ──────────────────────────────────────────────────
        ImGui::TextDisabled("Rendering");
        ImGui::Separator();

        bool solid = !m_WireframeMode;
        if (ImGui::RadioButton("Solid", solid))
            m_WireframeMode = false;
        ImGui::SameLine();
        if (ImGui::RadioButton("Wireframe", m_WireframeMode))
            m_WireframeMode = true;

        ImGui::Checkbox("Show Statistics", &m_ShowRenderingStatistics);

        ImGui::Spacing();

        // ── Transform Snapping ──────────────────────────────────────────
        ImGui::TextDisabled("Transform Snapping");
        ImGui::Separator();

        bool snapEnabled = m_ViewportPanel->GetSnapEnabled();
        if (ImGui::Checkbox("Enabled", &snapEnabled))
            m_ViewportPanel->SetSnapEnabled(snapEnabled);
        UI::SetTooltip("Keep snapping enabled. Hold Ctrl for temporary snapping.");

        Ref<EditorSettings> editorSettings = Editor::Get().GetEditorSettings();
        ImGui::PushItemWidth(settingsWidth * 0.55f);
        if (ImGui::DragFloat3("Move", glm::value_ptr(editorSettings->GridMoveSnap), 0.01f, 0.001f, 1000.0f, "%.3f m"))
            editorSettings->GridMoveSnap = glm::max(editorSettings->GridMoveSnap, glm::vec3(0.001f));
        if (ImGui::DragFloat("Rotate", &editorSettings->GridRotateSnap, 0.5f, 0.1f, 180.0f, "%.1f deg"))
            editorSettings->GridRotateSnap = std::max(editorSettings->GridRotateSnap, 0.1f);
        if (ImGui::DragFloat("Scale", &editorSettings->GridScaleSnap, 0.01f, 0.001f, 10.0f, "%.3f"))
            editorSettings->GridScaleSnap = std::max(editorSettings->GridScaleSnap, 0.001f);
        ImGui::PopItemWidth();

        ImGui::Spacing();

        // ── Grid ───────────────────────────────────────────────────────
        ImGui::TextDisabled("Grid");
        ImGui::Separator();

        ImGui::Checkbox("Show Grid", &m_ShowGrid);
        if (m_ShowGrid)
        {
            ImGui::Checkbox("Show Axes", &m_ShowGridAxes);

            ImGui::PushItemWidth(settingsWidth * 0.55f);
            ImGui::DragFloat("Fine Cell Size", &m_GridFineSize, 0.1f, 0.1f, 100.0f, "%.2f m");
            ImGui::DragFloat("Coarse Cell Size", &m_GridCoarseSize, 1.0f, 1.0f, 1000.0f, "%.1f m");
            ImGui::DragFloat("Line Width", &m_GridLineWidth, 0.001f, 0.005f, 0.49f, "%.3f");
            ImGui::DragFloat("Opacity", &m_GridOpacity, 0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::PopItemWidth();
        }

        ImGui::Spacing();

        // ── Physics Gizmos ─────────────────────────────────────────────
        ImGui::TextDisabled("Physics Gizmos");
        ImGui::Separator();

        ImGui::Checkbox("Show Colliders", &m_ShowColliders);
        if (m_ShowColliders)
        {
            ImGui::PushItemWidth(settingsWidth * 0.55f);
            ImGui::ColorEdit4("Collider Color", glm::value_ptr(m_ColliderColor), ImGuiColorEditFlags_NoInputs);
            ImGui::PopItemWidth();
        }

        ImGui::Spacing();
        ImGui::End();

        UI::PopID();
    }

    void EditorLayer::UI_Settings()
    {
        if (!m_ShowSettings)
            return;

        ImGui::SetNextWindowSize(ImVec2(620.0f, 680.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(420.0f, 320.0f), ImVec2(FLT_MAX, FLT_MAX));
        if (!ImGui::Begin("Settings", &m_ShowSettings))
        {
            ImGui::End();
            return;
        }
        UI::ScopedStyle spacing(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 7.0f));

        UIUtils::SearchWidget(m_SettingsSearch, "Search settings...");
        ImGui::Separator();

        const auto matchesSection = [this](std::initializer_list<const char*> terms) {
            if (m_SettingsSearch.empty())
                return true;
            for (const char* term : terms)
            {
                if (StringUtils::IsSearchMathing(term, m_SettingsSearch, false, true, true))
                    return true;
            }
            return false;
        };
        const auto beginSection = [this](const char* label, ImGuiTreeNodeFlags defaultFlags) {
            if (!m_SettingsSearch.empty())
                ImGui::SetNextItemOpen(true, ImGuiCond_Always);
            return ImGui::CollapsingHeader(label, defaultFlags);
        };

        if (matchesSection({ "startup project recent auto load" }) && beginSection("Startup", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Open the last project on startup", &m_AutoLoadLastProject);
            ImGui::TextDisabled("Crowny opens the project saved in editor settings.");
        }

        if (matchesSection({ "code editor IDE Visual Studio" }) && beginSection("Code editor", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const Vector<CodeEditorInstallation>& editors = CodeEditorManager::Get().GetAvailableEditors();
            if (editors.empty())
                ImGui::TextDisabled("No supported code editor was found.");
            else
            {
                m_VisualStudioVersionId = std::clamp(m_VisualStudioVersionId, 0, static_cast<int32_t>(editors.size() - 1));
                std::function<const String&(const CodeEditorInstallation&)> selector = [](const CodeEditorInstallation& install) -> const String& {
                    return install.Name;
                };
                // UI::BeginPropertyGrid() pushes a depth-based id and restarts the "##N" widget counter, so two sibling
                // property grids in one window (this one and the Time section) would hand out identical ids. Scope it.
                ImGui::PushID("CodeEditorSettings");
                UI::BeginPropertyGrid();
                if (UI::PropertyDropdown("Editor", editors, m_VisualStudioVersionId, selector))
                {
                    const CodeEditorInstallation& selectedEditor = editors[m_VisualStudioVersionId];
                    CodeEditorManager::Get().SetActive(selectedEditor.ExecutablePath);
                    if (Editor::Get().IsProjectLoaded())
                        CodeEditorManager::Get().SyncSolution(GAME_ASSEMBLY);
                }
                UI::EndPropertyGrid();
                ImGui::PopID();
            }
        }

        if (matchesSection({ "managed C# assembly dependency mono coreclr runtime" }) &&
            beginSection("Managed assemblies", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (!Editor::Get().IsProjectLoaded())
                ImGui::TextDisabled("Open a project to add managed assembly dependencies.");
            else
            {
                Vector<Path>& dependencies = Editor::Get().GetProjectSettings()->ManagedAssemblyReferences;
                ImGui::TextWrapped("Referenced DLLs are added to the generated C# project and validated before Mono or CoreCLR runs the game.");
                ImGui::Spacing();

                size_t removeIndex = dependencies.size();
                for (size_t index = 0; index < dependencies.size(); index++)
                {
                    ImGui::PushID(static_cast<int32_t>(index));
                    ImGui::TextUnformatted(dependencies[index].generic_string().c_str());
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Remove"))
                        removeIndex = index;
                    ImGui::PopID();
                }
                if (removeIndex != dependencies.size())
                {
                    dependencies.erase(dependencies.begin() + static_cast<ptrdiff_t>(removeIndex));
                    Editor::Get().SaveProjectSettings();
                    CodeEditorManager::Get().NotifyProjectSettingsChanged();
                }

                if (ImGui::Button("Add assembly..."))
                {
                    Vector<Path> selected;
                    const Path projectRoot = Editor::Get().GetProjectPath();
                    if (FileSystem::OpenFileDialog(FileDialogType::OpenFile, selected, "Choose managed assembly", projectRoot,
                                                   { { "Managed assembly", "*.dll" } }) &&
                        !selected.empty())
                    {
                        Path assembly = fs::absolute(selected.front()).lexically_normal();
                        const Path relative = assembly.lexically_relative(projectRoot);
                        if (!relative.empty() && relative != "." && !relative.generic_string().starts_with(".."))
                            assembly = relative;
                        if (std::find(dependencies.begin(), dependencies.end(), assembly) == dependencies.end())
                            dependencies.push_back(assembly);
                        Editor::Get().SaveProjectSettings();
                        CodeEditorManager::Get().NotifyProjectSettingsChanged();
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Synchronize code project"))
                    CodeEditorManager::Get().SyncSolution(GAME_ASSEMBLY);
            }
        }

        if (matchesSection({ "viewport grid wireframe collider rendering" }) && beginSection("Viewport", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::BeginTable("##ViewportSettings", 2,
                                  ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_NoPadOuterX))
            {
                ImGui::TableSetupColumn("Setting", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                const auto checkboxRow = [](const char* label, bool* value) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(label);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::PushID(label);
                    ImGui::Checkbox("##Value", value);
                    ImGui::PopID();
                };
                checkboxRow("Wireframe rendering", &m_WireframeMode);
                checkboxRow("Show rendering statistics", &m_ShowRenderingStatistics);
                checkboxRow("Show grid", &m_ShowGrid);
                checkboxRow("Show grid axes", &m_ShowGridAxes);
                checkboxRow("Show collider outlines", &m_ShowColliders);
                ImGui::EndTable();
            }
        }

        if (matchesSection({ "input actions bindings keyboard mouse gamepad controls rebinding" }) &&
            beginSection("Input actions", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (!Editor::Get().IsProjectLoaded())
                ImGui::TextDisabled("Open a project to edit its input actions.");
            else
            {
                InputMap& inputMap = Editor::Get().GetProjectSettings()->InputActions;
                if (InputSettingsEditor::Render(inputMap))
                    Input::SetActionMap(inputMap);
                ImGui::Spacing();
                if (ImGui::Button("Save input settings"))
                    Editor::Get().SaveProjectSettings();
                ImGui::SameLine();
                if (ImGui::Button("Clear runtime rebinds"))
                    Input::ClearActionRebinds();
            }
        }

        if (matchesSection({ "time scale fixed timestep maximum" }) && beginSection("Time", ImGuiTreeNodeFlags_DefaultOpen))
            UI_TimeSettings();

        if (matchesSection({ "physics 2D gravity solver layers collision matrix" }) && beginSection("Physics 2D", ImGuiTreeNodeFlags_None))
            UI_Physics2DSettings();

        if (matchesSection({ "workspace layout reset panels command palette" }) && beginSection("Workspace", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextDisabled("Reset docking to the default editor arrangement.");
            if (ImGui::Button("Reset layout"))
                ImGui::OpenPopup("Reset workspace layout?");
            if (ImGui::BeginPopupModal("Reset workspace layout?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextUnformatted("Open panels will move back to the default arrangement.");
                ImGui::Spacing();
                if (ImGui::Button("Reset", ImVec2(100.0f, 0.0f)))
                {
                    ResetWorkspaceLayout();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f)))
                    ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
        }

        if (matchesSection({ "developer debug diagnostics ImGui asset entity C#" }) && beginSection("Developer tools", ImGuiTreeNodeFlags_None))
        {
            ImGui::Checkbox("Show ImGui demo", &m_ShowDemoWindow);
#ifdef CW_DEBUG
            ImGui::Checkbox("Show C# debug info", &m_ShowScriptDebugInfo);
            ImGui::Checkbox("Show asset info", &m_ShowAssetInfo);
            ImGui::Checkbox("Show entity debug info", &m_ShowEntityDebugInfo);
#else
            ImGui::TextDisabled("More diagnostics are available in debug builds.");
#endif
        }

        if (!m_SettingsSearch.empty() &&
            !matchesSection({ "startup project recent auto load", "code editor IDE Visual Studio", "managed C# assembly dependency mono coreclr runtime",
                              "viewport grid wireframe collider rendering",
                              "time scale fixed timestep maximum", "physics 2D gravity solver layers collision matrix",
                              "input actions bindings keyboard mouse gamepad controls rebinding", "workspace layout reset panels command palette",
                              "developer debug diagnostics ImGui asset entity C#" }))
            ImGui::TextDisabled("No matching settings.");

        ImGui::End();
    }

} // namespace Crowny
