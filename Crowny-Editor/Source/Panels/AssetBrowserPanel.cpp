#include "cwepch.h"

#include "Panels/AssetBrowserPanel.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/PlatformUtils.h"
#include "Crowny/Common/StringUtils.h"

#include "Crowny/NodeGraph/NodeGraphAsset.h"
#include "Crowny/NodeGraph/NodeRegistry.h"

#include "Crowny/Input/Input.h"

#include "Editor/Editor.h"
#include "Editor/EditorAssets.h"
#include "Editor/EditorUtils.h"
#include "Editor/ProjectLibrary.h"
#include "Editor/Script/CodeEditor.h"

#include "Crowny/Assets/AssetManager.h"

#include "Crowny/Common/Constants.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/Renderer/Material.h"

#include "UI/UIUtils.h"

#include "Crowny/ImGui/ImGuiVulkanTexture.h"
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <spdlog/fmt/fmt.h>

namespace Crowny
{

    static String GetDefaultFileNameFromType(AssetBrowserItem type)
    {
        switch (type)
        {
        case AssetBrowserItem::CScript:
            return "New Script.cs";
        case AssetBrowserItem::Folder:
            return "New Folder";
        case AssetBrowserItem::Material:
            return "New Material.cwmat";
        case AssetBrowserItem::Prefab:
            return "New Prefab.cwprefab";
        case AssetBrowserItem::Shader:
            return "New Shader.shader";
        case AssetBrowserItem::ComputeShader:
            return "New ComputeShader.cshader";
        case AssetBrowserItem::PhysicsMaterial2D:
            return "New PhysicsMaterial2D.pmat";
        case AssetBrowserItem::PhysicsMaterial3D:
            return "New PhysicsMaterial3D.pmat3d";
        case AssetBrowserItem::RenderTexture:
            return "New RenderTexture.rt";
        case AssetBrowserItem::Scene:
            return "New Scene.cwscene";
        case AssetBrowserItem::NodeGraph:
            return "New NodeGraph.cwng";
        default:
            return "New File";
        }
    }

    static const char* GetAssetTypeName(AssetType type)
    {
        switch (type)
        {
        case AssetType::AudioClip:
            return "Audio";
        case AssetType::Texture:
            return "Texture";
        case AssetType::Shader:
            return "Shader";
        case AssetType::Material:
            return "Material";
        case AssetType::Mesh:
        case AssetType::MeshSource:
            return "Model";
        case AssetType::ScriptCode:
            return "Script";
        case AssetType::PhysicsMaterial2D:
        case AssetType::PhysicsMaterial:
            return "Physics material";
        case AssetType::PlainText:
            return "Text";
        case AssetType::Font:
            return "Font";
        case AssetType::Scene:
            return "Scene";
        case AssetType::NodeGraph:
            return "Node graph";
        case AssetType::EnvironmentMap:
            return "Environment";
        case AssetType::Prefab:
            return "Prefab";
        case AssetType::AudioMixer:
            return "Audio mixer";
        case AssetType::AnimationClip:
            return "Animation";
        default:
            return "File";
        }
    }

    static const char* GetEntryTypeName(const Ref<LibraryEntry>& entry)
    {
        if (entry->Type == LibraryEntryType::Directory)
            return "Folder";
        const FileEntry* fileEntry = static_cast<FileEntry*>(entry.get());
        return fileEntry->Metadata ? GetAssetTypeName(fileEntry->Metadata->Type) : "File";
    }

    static String FormatFileSize(uint32_t bytes)
    {
        if (bytes >= 1024 * 1024)
            return fmt::format("{:.1f} MB", static_cast<float>(bytes) / (1024.0f * 1024.0f));
        if (bytes >= 1024)
            return fmt::format("{:.1f} KB", static_cast<float>(bytes) / 1024.0f);
        return fmt::format("{} B", bytes);
    }

    static String FormatEntryTime(std::time_t timestamp)
    {
        if (timestamp == 0)
            return "Unknown";

        tm timeInfo{};
#ifdef CW_PLATFORM_WIN32
        localtime_s(&timeInfo, &timestamp);
#else
        localtime_r(&timestamp, &timeInfo);
#endif
        char buffer[32]{};
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", &timeInfo);
        return buffer;
    }

    static void DrawAssetTooltip(const Path& path, const AssetPreviewResult* preview)
    {
        if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            return;
        ImGui::BeginTooltip();
        const String pathText = path.string();
        ImGui::TextUnformatted(pathText.c_str());
        if (preview != nullptr)
        {
            // Worker threads populate preview metadata while the status remains Loading.
            // Only read it after the main thread has observed completion and published Ready.
            if (preview->Status == AssetPreviewStatus::Ready && !preview->Details.empty())
                ImGui::TextDisabled("%s", preview->Details.c_str());
            if (preview->Status == AssetPreviewStatus::Failed && !preview->Error.empty())
                ImGui::TextColored(ImVec4(0.95f, 0.38f, 0.32f, 1.0f), "Preview unavailable: %s", preview->Error.c_str());
            else if (preview->Status == AssetPreviewStatus::Queued || preview->Status == AssetPreviewStatus::Loading)
                ImGui::TextDisabled("Generating preview...");
        }
        ImGui::EndTooltip();
    }

    AssetBrowserPanel::AssetBrowserPanel(const String& name, std::function<void(const Path&)> selectedPathCallback)
      : ImGuiPanel(name), m_SetSelectedPathCallback(selectedPathCallback)
    {
        m_CsDefaultText = EditorAssets::GetDefaultScriptTemplate();
        m_FolderIcon = ImGuiVulkanTexture::Get(EditorAssets::Get().FolderIcon);
        m_FileIcon = ImGuiVulkanTexture::Get(EditorAssets::Get().FileIcon);
    }

    void AssetBrowserPanel::Initialize()
    {
        if (Editor::Get().GetProjectSettings()->LastAssetBrowserSelectedEntry.empty())
            m_CurrentDirectoryEntry = ProjectLibrary::Get().GetRoot().get();
        else
        {
            LibraryEntry* entry = ProjectLibrary::Get().FindEntry(Editor::Get().GetProjectSettings()->LastAssetBrowserSelectedEntry).get();
            if (entry == nullptr || entry->Type == LibraryEntryType::File)
                m_CurrentDirectoryEntry = ProjectLibrary::Get().GetRoot().get();
            else
                m_CurrentDirectoryEntry = static_cast<DirectoryEntry*>(entry);
        }
        RecalculateDirectoryEntries();

        UpdateDisplayList();
    }

    void AssetBrowserPanel::Unload()
    {
        m_CurrentDirectoryEntry = nullptr;
        m_DirectoryPathEntries.clear();
        m_DisplayList.clear();
        m_PreviewService.Clear();
        m_BackwardHistory = Stack<DirectoryEntry*>();
        m_ForwardHistory = Stack<DirectoryEntry*>();
    }

    void AssetBrowserPanel::Render()
    {
        m_PreviewService.Update();
        UI::ScopedStyle windowPadding(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 2.0f));
        if (!BeginPanel(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            EndPanel();
            return;
        }

        DrawHeader();
        ImGui::Separator();

        const float statusHeight = ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild("AssetBrowser", ImVec2(0, -statusHeight), false, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoNav);

        // Right click not on a file
        if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_NoOpenOverExistingPopup | ImGuiPopupFlags_MouseButtonRight))
        {
            ShowContextMenuContents();
            ImGui::EndPopup();
        }

        // Files
        DrawFiles();

        ImGui::EndChild();
        ImGui::Separator();
        DrawStatusBar();
        EndPanel();
        DrawTreeView();
    }

    void AssetBrowserPanel::SetCurrentDirectory(DirectoryEntry* entry)
    {
        m_PreviewService.CancelPending();
        m_ForwardHistory = {};
        m_BackwardHistory.push(m_CurrentDirectoryEntry);
        m_CurrentDirectoryEntry = entry;

        RecalculateDirectoryEntries();
        ClearSelection();

        m_RenamingPath.clear();
        m_RenamingText.clear();
        m_RequiresSort = true;
        UpdateDisplayList();
    }

    void AssetBrowserPanel::GoBackward()
    {
        if (m_BackwardHistory.empty())
            return;

        m_PreviewService.CancelPending();
        m_ForwardHistory.push(m_CurrentDirectoryEntry);
        m_CurrentDirectoryEntry = m_BackwardHistory.top();
        RecalculateDirectoryEntries();
        m_BackwardHistory.pop();
        ClearSelection();

        m_RenamingPath.clear();
        m_RenamingText.clear();
        m_RequiresSort = true;
        UpdateDisplayList();
    }

    void AssetBrowserPanel::GoForward()
    {
        if (m_ForwardHistory.empty())
            return;

        m_PreviewService.CancelPending();
        m_BackwardHistory.push(m_CurrentDirectoryEntry);
        m_CurrentDirectoryEntry = m_ForwardHistory.top();
        RecalculateDirectoryEntries();
        m_ForwardHistory.pop();
        ClearSelection();

        m_RenamingPath.clear();
        m_RenamingText.clear();
        m_RequiresSort = true;
        UpdateDisplayList();
    }

    void AssetBrowserPanel::DrawHeader()
    {
        if (m_CurrentDirectoryEntry == nullptr)
            return;

        UI::ScopedStyle itemSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 6.0f));
        UI::ScopedStyle framePadding(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
        const ImGuiTableFlags tableFlags =
          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoBordersInBody;

        if (!ImGui::BeginTable("##assetBrowserNavigation", 2, tableFlags))
            return;
        ImGui::TableSetupColumn("Controls", ImGuiTableColumnFlags_WidthFixed, 132.0f);
        ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);

        if (!m_BackwardHistory.empty())
        {
            if (ImGui::ArrowButton("##assetBack", ImGuiDir_Left))
                GoBackward();
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::ArrowButton("##assetBack", ImGuiDir_Left);
            ImGui::EndDisabled();
        }
        UI::SetTooltip("Back");
        ImGui::SameLine();

        if (!m_ForwardHistory.empty())
        {
            if (ImGui::ArrowButton("##assetForward", ImGuiDir_Right))
                GoForward();
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::ArrowButton("##assetForward", ImGuiDir_Right);
            ImGui::EndDisabled();
        }
        UI::SetTooltip("Forward");
        ImGui::SameLine();

        if (m_CurrentDirectoryEntry == ProjectLibrary::Get().GetRoot().get())
        {
            ImGui::BeginDisabled();
            ImGui::ArrowButton("##assetUp", ImGuiDir_Up);
            ImGui::EndDisabled();
        }
        else
        {
            if (ImGui::ArrowButton("##assetUp", ImGuiDir_Up))
                SetCurrentDirectory(m_CurrentDirectoryEntry->Parent);
        }
        UI::SetTooltip("Parent folder");
        ImGui::SameLine();

        const bool importing = ProjectLibrary::Get().IsImporting();
        if (importing)
            ImGui::BeginDisabled();
        if (ImGui::Button("Reload##assets"))
            ProjectLibrary::Get().RefreshAsync(m_CurrentDirectoryEntry->Filepath);
        if (importing)
            ImGui::EndDisabled();
        UI::SetTooltip(importing ? "An import is already running" : "Rescan this folder");

        if (importing)
            UpdateDisplayList(); // Show newly imported assets as they complete

        ImGui::TableSetColumnIndex(1);
        ImGui::BeginChild("##assetBreadcrumbs", ImVec2(0.0f, ImGui::GetFrameHeight()), false, ImGuiWindowFlags_HorizontalScrollbar);
        for (size_t i = 0; i < m_DirectoryPathEntries.size(); i++)
        {
            DirectoryEntry* dirEntry = m_DirectoryPathEntries[i];
            ImGui::PushID(static_cast<int>(i));
            if (i > 0)
            {
                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled(">");
                ImGui::SameLine();
            }

            const bool current = i + 1 == m_DirectoryPathEntries.size();
            if (current)
            {
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(dirEntry->ElementName.c_str());
            }
            else if (ImGui::SmallButton(dirEntry->ElementName.c_str()))
            {
                SetCurrentDirectory(dirEntry);
                ImGui::PopID();
                break;
            }
            UI::SetTooltip(dirEntry->Filepath.string());
            if (!current)
                ImGui::SameLine();
            ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::EndTable();

        static const char* filterLabels[] = { "All assets", "Scenes", "Images", "Materials", "Models", "Audio", "Code" };
        static const char* sortingLabels[] = { "Name", "Size", "Date" };
        const auto drawSearch = [&]() {
            ImGui::SetNextItemWidth(-1.0f);
            if (UIUtils::SearchWidget(m_SearchString, "Search this folder..."))
                UpdateDisplayList();
        };
        const auto drawFilter = [&]() {
            ImGui::SetNextItemWidth(-1.0f);
            const uint32_t currentFilter = static_cast<uint32_t>(m_AssetFilter);
            if (ImGui::BeginCombo("##assetTypeFilter", filterLabels[currentFilter]))
            {
                for (uint32_t i = 0; i < static_cast<uint32_t>(AssetBrowserFilter::Count); i++)
                {
                    if (ImGui::Selectable(filterLabels[i], i == currentFilter))
                    {
                        m_AssetFilter = static_cast<AssetBrowserFilter>(i);
                        ClearSelection();
                        UpdateDisplayList();
                    }
                }
                ImGui::EndCombo();
            }
            UI::SetTooltip("Filter by asset type");
        };
        const auto drawSort = [&]() {
            ImGui::SetNextItemWidth(-1.0f);
            const uint32_t currentMode = static_cast<uint32_t>(m_FileSortingMode);
            if (ImGui::BeginCombo("##assetSort", sortingLabels[currentMode]))
            {
                for (uint32_t i = 0; i < static_cast<uint32_t>(FileSortingMode::SortCount); i++)
                {
                    if (ImGui::Selectable(sortingLabels[i], i == currentMode))
                    {
                        m_FileSortingMode = static_cast<FileSortingMode>(i);
                        m_RequiresSort = true;
                    }
                }
                ImGui::EndCombo();
            }
            UI::SetTooltip("Sort assets");
        };
        const auto drawView = [&]() {
            const bool gridView = m_View == AssetBrowserView::Grid;
            if (gridView)
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button("Grid"))
                m_View = AssetBrowserView::Grid;
            if (gridView)
                ImGui::PopStyleColor();
            UI::SetTooltip("Thumbnail grid");
            ImGui::SameLine();
            if (!gridView)
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button("List"))
                m_View = AssetBrowserView::List;
            if (!gridView)
                ImGui::PopStyleColor();
            UI::SetTooltip("Detailed list");
        };
        const auto drawThumbnailSize = [&]() {
            ImGui::SetNextItemWidth(-1.0f);
            float thumbnailChange = m_ThumbnailSize;
            if (ImGui::SliderFloat("##assetIconSize", &thumbnailChange, MIN_ASSET_THUMBNAIL_SIZE, MAX_ASSET_THUMBNAIL_SIZE, "%.0f px"))
                m_ThumbnailSize = thumbnailChange;
            UI::SetTooltip("Thumbnail size");
        };

        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const bool compact = availableWidth < 680.0f;
        const int controlColumns = m_View == AssetBrowserView::Grid ? 4 : 3;
        if (!compact)
        {
            if (ImGui::BeginTable("##assetBrowserToolbar", controlColumns + 1, tableFlags))
            {
                ImGui::TableSetupColumn("Search", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Sort", ImGuiTableColumnFlags_WidthFixed, 108.0f);
                ImGui::TableSetupColumn("View", ImGuiTableColumnFlags_WidthFixed, 92.0f);
                if (m_View == AssetBrowserView::Grid)
                    ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 125.0f);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                drawSearch();
                ImGui::TableSetColumnIndex(1);
                drawFilter();
                ImGui::TableSetColumnIndex(2);
                drawSort();
                ImGui::TableSetColumnIndex(3);
                drawView();
                if (m_View == AssetBrowserView::Grid)
                {
                    ImGui::TableSetColumnIndex(4);
                    drawThumbnailSize();
                }
                ImGui::EndTable();
            }
        }
        else
        {
            drawSearch();
            if (ImGui::BeginTable("##assetBrowserCompactToolbar", availableWidth < 430.0f ? 2 : controlColumns, tableFlags))
            {
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Sort", ImGuiTableColumnFlags_WidthStretch);
                if (availableWidth >= 430.0f)
                {
                    ImGui::TableSetupColumn("View", ImGuiTableColumnFlags_WidthFixed, 92.0f);
                    if (m_View == AssetBrowserView::Grid)
                        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                }
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                drawFilter();
                ImGui::TableSetColumnIndex(1);
                drawSort();
                if (availableWidth < 430.0f)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    drawView();
                    if (m_View == AssetBrowserView::Grid)
                    {
                        ImGui::TableSetColumnIndex(1);
                        drawThumbnailSize();
                    }
                }
                else
                {
                    ImGui::TableSetColumnIndex(2);
                    drawView();
                    if (m_View == AssetBrowserView::Grid)
                    {
                        ImGui::TableSetColumnIndex(3);
                        drawThumbnailSize();
                    }
                }
                ImGui::EndTable();
            }
        }
    }

    void AssetBrowserPanel::HandleKeyboardNavigation()
    {
        // Disable keyboard stuff if ImGui wants to use the keyboard (for example in InputText widgets)
        if (ImGui::GetIO().WantCaptureKeyboard)
            return;

        const DisplayList& displayList = GetDisplayList();

        if (Input::IsKeyDown(Key::Delete)) // Delete selected items
        {
            while (!m_SelectionSet.empty())
            {
                const DisplayList& displayList = GetDisplayList();
                for (const Ref<LibraryEntry>& entry : displayList)
                {
                    // This is not enough
                    auto iterFind = m_SelectionSet.find(entry->ElementNameHash);
                    if (iterFind != m_SelectionSet.end())
                    {
                        ProjectLibrary::Get().DeleteEntry(entry->Filepath);
                        m_SelectionSet.erase(iterFind);
                        break;
                    }
                }
            }
            UpdateDisplayList();
            ClearSelection();
        }

        if (Input::IsKeyDown(Key::F2)) // Rename the first selected item
        {
            if (m_SelectionStartIndex >= displayList.size())
                return;
            const Ref<LibraryEntry>& entry = displayList[m_SelectionStartIndex];
            if (!entry)
                return;
            m_RenamingPath = entry->Filepath; // TODO: Use hash instead of path
            m_RenamingText = m_RenamingPath.filename().string();
        }

        if (Input::IsKeyDown(Key::Enter) && !m_SelectionSet.empty()) // Enter a directory using the keyboard
        {
            LibraryEntry* entry = displayList[m_SelectionStartIndex].get();
            HandleOpen(entry);
        }

        if (Input::IsKeyDown(Key::Backspace) || Input::IsMouseButtonDown(Mouse::Button3)) // Go back
            GoBackward();

        if (Input::IsMouseButtonDown(Mouse::Button4)) // Go forward
            GoForward();

        if (Input::IsKeyPressed(Key::LeftControl))
        {
            if (Input::IsKeyDown(Key::C)) // Copy (Ctrl+C)
            {
                String clipboardString;
                for (const auto& entry : displayList)
                {
                    if (m_SelectionSet.find(entry->ElementNameHash) != m_SelectionSet.end())
                        clipboardString += entry->Filepath.string() + '\n';
                }
                clipboardString = clipboardString.substr(1, clipboardString.size() - 1);
                PlatformUtils::CopyToClipboard(clipboardString);
            }

            if (Input::IsKeyDown(Key::V) && m_SearchString.empty()) // Paste (Ctrl+V), only paste if we aren't searching
            {
                // TODO: Is this really a path?
                String clipboard = PlatformUtils::CopyFromClipboard();
                Vector<String> paths = StringUtils::SplitString(clipboard, "\n");
                for (const auto& path : paths) // Maybe here I would need to remove the last char
                    ProjectLibrary::Get().CopyEntry(path, EditorUtils::GetUniquePath(m_CurrentDirectoryEntry->Filepath / Path(path).filename()));
                UpdateDisplayList();
            }

            if (Input::IsKeyDown(Key::R)) // Refresh (Ctrl+R)
            {
                ProjectLibrary::Get().Refresh(m_CurrentDirectoryEntry->Filepath);
                UpdateDisplayList();
            }

            if (Input::IsKeyDown(Key::A)) // Select all (Ctrl+A)
            {
                m_SelectionStartIndex = 0;
                m_SelectionEndIndex = displayList.size() > 0 ? (uint32_t)(displayList.size() - 1) : 0;
                for (uint32_t i = m_SelectionStartIndex; i <= m_SelectionEndIndex; i++)
                    m_SelectionSet.insert(displayList[i]->ElementNameHash);
            }
        }

        // Keyboard navigation
        if (m_SelectionSet.empty()) // Select from unselected state
        {
            if (Input::IsKeyUp(Key::Left) || Input::IsKeyUp(Key::Up))
            {
                if (displayList.size() > 0)
                {
                    m_SelectionSet.insert(displayList[0]->ElementNameHash); // Select the first entry
                    m_SelectionEndIndex = m_SelectionStartIndex = 0;
                }
            }
            if (Input::IsKeyUp(Key::Right) || Input::IsKeyUp(Key::Down))
            {
                if (displayList.size() > 0)
                {
                    const size_t lastIdx = displayList.size() - 1;
                    m_SelectionSet.insert(displayList[lastIdx]->ElementNameHash); // Select the last entry
                    m_SelectionEndIndex = m_SelectionStartIndex = (uint32_t)lastIdx;
                }
            }
        }
        else
        {
            bool shiftSelectionChanged = false;
            if (Input::IsKeyDown(Key::Left))
            {
                if (!Input::IsKeyPressed(Key::LeftShift))
                {
                    m_SelectionSet.clear();
                    m_SelectionEndIndex = m_SelectionStartIndex = std::max(0, (int32_t)m_SelectionStartIndex - 1);
                    const Ref<LibraryEntry>& entry = displayList[m_SelectionStartIndex];
                    m_SelectionSet.insert(entry->ElementNameHash);
                    m_SetSelectedPathCallback(entry->Filepath);
                }
                else
                {
                    m_SelectionEndIndex = std::max(0, (int32_t)m_SelectionEndIndex - 1);
                    const Ref<LibraryEntry>& entry = displayList[m_SelectionEndIndex];
                    m_SetSelectedPathCallback(entry->Filepath);
                    shiftSelectionChanged = true;
                }
            }
            if (Input::IsKeyDown(Key::Right))
            {
                if (!Input::IsKeyPressed(Key::LeftShift))
                {
                    m_SelectionSet.clear();
                    m_SelectionEndIndex = m_SelectionStartIndex = std::min((int32_t)m_SelectionStartIndex + 1, (int32_t)displayList.size() - 1);
                    const Ref<LibraryEntry>& entry = displayList[m_SelectionStartIndex];
                    m_SelectionSet.insert(entry->ElementNameHash);
                    m_SetSelectedPathCallback(entry->Filepath);
                }
                else
                {
                    m_SelectionEndIndex = std::min((int32_t)m_SelectionEndIndex + 1, (int32_t)displayList.size() - 1);
                    const Ref<LibraryEntry>& entry = displayList[m_SelectionEndIndex];
                    m_SetSelectedPathCallback(entry->Filepath);
                    shiftSelectionChanged = true;
                }
            }
            if (Input::IsKeyDown(Key::Up))
            {
                if (!Input::IsKeyPressed(Key::LeftShift))
                {
                    m_SelectionSet.clear();
                    m_SelectionEndIndex = m_SelectionStartIndex = std::max(0, (int32_t)(m_SelectionStartIndex - m_ColumnCount));
                    const Ref<LibraryEntry>& entry = displayList[m_SelectionStartIndex];
                    m_SelectionSet.insert(entry->ElementNameHash);
                    m_SetSelectedPathCallback(entry->Filepath);
                }
                else
                {
                    m_SelectionEndIndex = std::max(0, (int32_t)(m_SelectionEndIndex - m_ColumnCount));
                    const Ref<LibraryEntry>& entry = displayList[m_SelectionEndIndex];
                    m_SetSelectedPathCallback(entry->Filepath);
                    shiftSelectionChanged = true;
                }
            }
            if (Input::IsKeyDown(Key::Down))
            {
                if (!Input::IsKeyPressed(Key::LeftShift))
                {
                    m_SelectionSet.clear();
                    m_SelectionEndIndex = m_SelectionStartIndex = std::min(m_SelectionStartIndex + m_ColumnCount, (uint32_t)displayList.size() - 1);
                    const Ref<LibraryEntry>& entry = displayList[m_SelectionStartIndex];
                    m_SelectionSet.insert(entry->ElementNameHash);
                    m_SetSelectedPathCallback(entry->Filepath);
                }
                else
                {
                    m_SelectionEndIndex = std::min(m_SelectionEndIndex + m_ColumnCount, (uint32_t)displayList.size() - 1);

                    const Ref<LibraryEntry>& entry = displayList[m_SelectionEndIndex];
                    m_SetSelectedPathCallback(entry->Filepath);
                    shiftSelectionChanged = true;
                }
            }
            if (shiftSelectionChanged)
            {
                m_SelectionSet.clear();
                const uint32_t startIdx = std::min(m_SelectionStartIndex, m_SelectionEndIndex);
                const uint32_t endIdx = std::max(m_SelectionStartIndex, m_SelectionEndIndex);
                for (uint32_t i = startIdx; i <= endIdx; i++)
                    m_SelectionSet.insert(displayList[i]->ElementNameHash);
            }
        }
    }

    void AssetBrowserPanel::ClearSelection()
    {
        m_SelectionSet.clear();
        m_SelectionStartIndex = (uint32_t)-1;
        m_SelectionEndIndex = 0;

        m_SetSelectedPathCallback({});
    }

    void AssetBrowserPanel::SortDisplayList(DisplayList& displayList) const
    {
        std::sort(displayList.begin(), displayList.end(), [this](const Ref<LibraryEntry>& l, const Ref<LibraryEntry>& r) {
            if (m_FileSortingMode == FileSortingMode::SortByName)
            {
                if (l->Type == r->Type)
                    return StringUtils::CaseInsensitiveCompare(l->ElementName, r->ElementName);
                return (int32_t)l->Type < (int32_t)r->Type;
            }
            else if (m_FileSortingMode == FileSortingMode::SortByDate)
                return l->LastUpdateTime < r->LastUpdateTime;
            else if (m_FileSortingMode == FileSortingMode::SortBySize)
            {
                if (l->Type == r->Type && l->Type == LibraryEntryType::File)
                    return static_cast<FileEntry*>(l.get())->Filesize < static_cast<FileEntry*>(r.get())->Filesize;
                return (int32_t)l->Type < (int32_t)r->Type;
            }
            return false;
        });
    }

    Vector<AssetType> AssetBrowserPanel::GetActiveAssetTypes() const
    {
        switch (m_AssetFilter)
        {
        case AssetBrowserFilter::Scenes:
            return { AssetType::Scene, AssetType::Prefab };
        case AssetBrowserFilter::Images:
            return { AssetType::Texture, AssetType::EnvironmentMap };
        case AssetBrowserFilter::Materials:
            return { AssetType::Material, AssetType::PhysicsMaterial, AssetType::PhysicsMaterial2D };
        case AssetBrowserFilter::Models:
            return { AssetType::Mesh, AssetType::MeshSource, AssetType::AnimationClip };
        case AssetBrowserFilter::Audio:
            return { AssetType::AudioClip, AssetType::AudioMixer };
        case AssetBrowserFilter::Code:
            return { AssetType::ScriptCode, AssetType::Shader, AssetType::NodeGraph, AssetType::PlainText };
        default:
            return {};
        }
    }

    const AssetBrowserPanel::DisplayList& AssetBrowserPanel::GetDisplayList()
    {
        DisplayList& displayList = m_DisplayList;
        if (m_RequiresSort)
        {
            // Store the selection start idx hash so it can be restored after the entries move around.
            size_t selectionStartHash = 0;
            if (m_SelectionStartIndex < displayList.size())
                selectionStartHash = displayList[m_SelectionStartIndex]->ElementNameHash;
            SortDisplayList(displayList);
            m_RequiresSort = false;
            if (m_SelectionStartIndex < displayList.size())
            {
                for (uint32_t i = 0; i < displayList.size(); i++)
                {
                    if (displayList[i]->ElementNameHash == selectionStartHash)
                        m_SelectionStartIndex = m_SelectionEndIndex = i;
                }
            }
        }
        return displayList;
    }

    // Currently the search is performed again. Since we kinda know the changes this might not be necessary.
    void AssetBrowserPanel::UpdateDisplayList()
    {
        if (m_CurrentDirectoryEntry == nullptr)
            return;

        const Vector<AssetType> assetTypes = GetActiveAssetTypes();
        if (!m_SearchString.empty())
        {
            const String wildcardSearch = "*" + m_SearchString + "*";
            m_DisplayList = ProjectLibrary::Get().Search(wildcardSearch, assetTypes, Ref<DirectoryEntry>(m_CurrentDirectoryEntry));
        }
        else
        {
            m_DisplayList.clear();
            m_DisplayList.reserve(m_CurrentDirectoryEntry->Children.size());
            for (const Ref<LibraryEntry>& entry : m_CurrentDirectoryEntry->Children)
            {
                if (assetTypes.empty())
                {
                    m_DisplayList.push_back(entry);
                    continue;
                }

                if (entry->Type != LibraryEntryType::File)
                    continue;
                const FileEntry* fileEntry = static_cast<FileEntry*>(entry.get());
                if (fileEntry->Metadata && std::find(assetTypes.begin(), assetTypes.end(), fileEntry->Metadata->Type) != assetTypes.end())
                    m_DisplayList.push_back(entry);
            }
        }
        m_RequiresSort = true;
    }

    void AssetBrowserPanel::HandleOpen(LibraryEntry* entry)
    {
        if (entry->Type == LibraryEntryType::Directory)
            SetCurrentDirectory(static_cast<DirectoryEntry*>(entry));
        else // Open the file
        {
            FileEntry* const fileEntry = static_cast<FileEntry*>(entry);
            const AssetType assetType = fileEntry->Metadata ? fileEntry->Metadata->Type : AssetType::None;
            if (assetType == AssetType::ScriptCode || assetType == AssetType::Shader)
                CodeEditorManager::Get().OpenFile(fileEntry->Filepath);
            else
                PlatformUtils::OpenExternally(entry->Filepath);
        }
    }

    void AssetBrowserPanel::DrawFiles()
    {
        const float cellSize = m_ThumbnailSize + m_Padding;
        const float panelWidth = ImGui::GetContentRegionAvail().x;
        m_ColumnCount = (int)(panelWidth / cellSize);
        if (m_ColumnCount < 1)
            m_ColumnCount = 1;

        bool dropping = false;
        bool hovered = false;

        if (m_CurrentDirectoryEntry == nullptr)
            return;

        const DisplayList& displayList = GetDisplayList();
        if (displayList.empty())
        {
            const bool constrained = !m_SearchString.empty() || m_AssetFilter != AssetBrowserFilter::All;
            const char* message = constrained ? "No assets match these filters." : "This folder is empty.";
            const float messageWidth = ImGui::CalcTextSize(message).x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (panelWidth - messageWidth) * 0.5f));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 24.0f);
            ImGui::TextDisabled("%s", message);
            if (constrained)
            {
                const char* clearLabel = "Clear search and filter";
                const float clearWidth = ImGui::CalcTextSize(clearLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (panelWidth - clearWidth) * 0.5f));
                if (ImGui::Button(clearLabel))
                {
                    m_SearchString.clear();
                    m_AssetFilter = AssetBrowserFilter::All;
                    UpdateDisplayList();
                }
            }
            else
            {
                const char* hint = "Right-click here to create an asset.";
                const float hintWidth = ImGui::CalcTextSize(hint).x;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (panelWidth - hintWidth) * 0.5f));
                ImGui::TextDisabled("%s", hint);
            }
            return;
        }

        if (ImGui::IsWindowFocused() && (m_RenamingPath.empty() || m_RenamingPath != m_CurrentDirectoryEntry->Filepath))
            HandleKeyboardNavigation();

        if (m_View == AssetBrowserView::List)
        {
            const ImGuiTableFlags listFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable |
                                              ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings;
            if (!ImGui::BeginTable("##assetList", 4, listFlags))
                return;

            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.52f);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.18f);
            ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthStretch, 0.20f);
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthStretch, 0.10f);
            ImGui::TableHeadersRow();

            const float iconSize = ImGui::GetTextLineHeight();
            const float rowHeight = ImGui::GetFrameHeight() + 4.0f;
            for (uint32_t entryIdx = 0; entryIdx < displayList.size(); entryIdx++)
            {
                const Ref<LibraryEntry>& entry = displayList[entryIdx];
                const Path& path = entry->Filepath;
                const uint32_t upperBits = static_cast<uint32_t>(entry->ElementNameHash >> 32);
                const uint32_t lowerBits = static_cast<uint32_t>(entry->ElementNameHash & 0xffffffff);
                ImGui::PushID(upperBits ^ lowerBits);

                const bool selected = m_SelectionSet.find(entry->ElementNameHash) != m_SelectionSet.end();
                ImTextureID texture = entry->Type == LibraryEntryType::Directory ? m_FolderIcon : m_FileIcon;
                const AssetPreviewResult* preview = nullptr;
                if (entry->Type == LibraryEntryType::File)
                {
                    preview = m_PreviewService.Request(*static_cast<FileEntry*>(entry.get()), 128);
                    if (preview != nullptr && preview->Status == AssetPreviewStatus::Ready && preview->Image)
                        texture = ImGuiVulkanTexture::Get(preview->Image);
                }

                ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
                ImGui::TableSetColumnIndex(0);
                const bool clicked = ImGui::Selectable(
                  "##assetRow", selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0.0f, rowHeight));
                const bool shouldOpen = clicked && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
                const bool rowHovered = ImGui::IsItemHovered();
                const ImVec2 rowMin = ImGui::GetItemRectMin();
                DrawAssetTooltip(path, preview);

                if (clicked)
                {
                    if (Input::IsKeyPressed(Key::LeftControl))
                    {
                        if (selected)
                            m_SelectionSet.erase(entry->ElementNameHash);
                        else
                        {
                            if (m_SelectionSet.empty())
                                m_SelectionStartIndex = entryIdx;
                            m_SelectionSet.insert(entry->ElementNameHash);
                            m_SelectionEndIndex = entryIdx;
                        }
                    }
                    else if (Input::IsKeyPressed(Key::LeftShift) && m_SelectionStartIndex != static_cast<uint32_t>(-1))
                    {
                        m_SelectionSet.clear();
                        const uint32_t first = std::min(entryIdx, m_SelectionStartIndex);
                        const uint32_t last = std::max(entryIdx, m_SelectionStartIndex);
                        for (uint32_t i = first; i <= last; i++)
                            m_SelectionSet.insert(displayList[i]->ElementNameHash);
                        m_SelectionEndIndex = entryIdx;
                    }
                    else
                    {
                        m_SelectionSet.clear();
                        m_SelectionSet.insert(entry->ElementNameHash);
                        m_SelectionStartIndex = m_SelectionEndIndex = entryIdx;
                        m_SetSelectedPathCallback(entry->Type == LibraryEntryType::File ? path : Path{});
                    }
                }

                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                {
                    UIUtils::SetAssetPayload(entry.get());
                    ImGui::Image(texture, ImVec2(32.0f, 32.0f), { 0, 1 }, { 1, 0 });
                    ImGui::SameLine();
                    ImGui::TextUnformatted(entry->ElementName.c_str());
                    ImGui::EndDragDropSource();
                }

                if (entry->Type == LibraryEntryType::Directory && ImGui::BeginDragDropTarget())
                {
                    dropping = true;
                    if (const FileEntry* fileEntry = UIUtils::AcceptAssetPayload())
                    {
                        ProjectLibrary::Get().MoveEntry(fileEntry->Filepath, path / fileEntry->Filepath.filename());
                        UpdateDisplayList();
                    }
                    ImGui::EndDragDropTarget();
                }

                if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && !selected)
                {
                    m_SelectionSet.clear();
                    m_SelectionSet.insert(entry->ElementNameHash);
                    m_SelectionStartIndex = m_SelectionEndIndex = entryIdx;
                    m_SetSelectedPathCallback(entry->Type == LibraryEntryType::File ? path : Path{});
                }
                if (!dropping && ImGui::BeginPopupContextItem("##assetListContext"))
                {
                    ShowContextMenuContents(entry.get());
                    ImGui::EndPopup();
                }

                if (entryIdx == m_SelectionEndIndex)
                    ImGui::ScrollToItem(ImGuiScrollFlags_KeepVisibleEdgeY);
                hovered |= rowHovered;

                ImGui::SetCursorScreenPos(ImVec2(rowMin.x + 6.0f, rowMin.y + (rowHeight - iconSize) * 0.5f));
                ImGui::Image(texture, ImVec2(iconSize, iconSize), { 0, 1 }, { 1, 0 });
                ImGui::SameLine();
                if (m_RenamingPath == path)
                {
                    const auto completeRename = [&]() {
                        if (m_RenamingPath.filename() != m_RenamingText)
                        {
                            const Path newPath = EditorUtils::GetUniquePath(m_RenamingPath.parent_path() / m_RenamingText);
                            ProjectLibrary::Get().MoveEntry(m_RenamingPath, newPath);
                            UpdateDisplayList();
                        }
                        m_RenamingPath.clear();
                        m_RenamingText.clear();
                    };
                    ImGui::SetNextItemWidth(std::max(80.0f, ImGui::GetColumnWidth() - iconSize - 24.0f));
                    ImGui::SetKeyboardFocusHere();
                    if (ImGui::InputText("##RenameAssetList", &m_RenamingText,
                                         ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
                        completeRename();
                    else if (ImGui::IsItemDeactivatedAfterEdit())
                        completeRename();
                    if (Input::IsKeyPressed(Key::Escape))
                        completeRename();
                }
                else
                    ImGui::TextUnformatted(entry->ElementName.c_str());

                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled("%s", GetEntryTypeName(entry));
                ImGui::TableSetColumnIndex(2);
                ImGui::TextDisabled("%s", FormatEntryTime(entry->LastUpdateTime).c_str());
                ImGui::TableSetColumnIndex(3);
                if (entry->Type == LibraryEntryType::File)
                    ImGui::TextDisabled("%s", FormatFileSize(static_cast<FileEntry*>(entry.get())->Filesize).c_str());
                else
                    ImGui::TextDisabled("-");

                ImGui::PopID();
                if (shouldOpen)
                {
                    ImGui::EndTable();
                    HandleOpen(entry.get());
                    return;
                }
            }

            ImGui::EndTable();
            if (Input::IsMouseButtonDown(Mouse::ButtonLeft) && !hovered && ImGui::IsWindowHovered())
                ClearSelection();
            return;
        }

        const ImGuiTableFlags tableFlags =
          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoBordersInBody;
        if (!ImGui::BeginTable("##assetGrid", m_ColumnCount, tableFlags))
            return;

        // Files
        for (uint32_t entryIdx = 0; entryIdx < displayList.size(); entryIdx++)
        {
            ImGui::TableNextColumn();
            const auto entry = displayList[entryIdx];
            const auto& path = entry->Filepath;

            const uint32_t upperBits = static_cast<uint32_t>(entry->ElementNameHash >> 32);
            const uint32_t lowerBits = static_cast<uint32_t>(entry->ElementNameHash & 0xffffffff);
            ImGui::PushID(upperBits ^ lowerBits);

            auto iterFind = m_SelectionSet.find(entry->ElementNameHash); // Show selected files
            const bool selected = iterFind != m_SelectionSet.end();

            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            if (!selected)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImTextureID tid;
            const AssetPreviewResult* preview = nullptr;
            if (entry->Type == LibraryEntryType::Directory)
                tid = m_FolderIcon;
            else
            {
                preview = m_PreviewService.Request(*static_cast<FileEntry*>(entry.get()), 128);
                tid = preview != nullptr && preview->Status == AssetPreviewStatus::Ready && preview->Image
                        ? ImGuiVulkanTexture::Get(preview->Image)
                        : m_FileIcon;
            }

            // Thumbnail
            ImGui::BeginGroup();
            ImGui::ImageButton("##thumb", tid, { m_ThumbnailSize, m_ThumbnailSize }, { 0, 1 }, { 1, 0 });
            ImGui::SetNextItemWidth(m_ThumbnailSize);
            if (m_RenamingPath.empty() || m_RenamingPath != path) // File icon
            {
                float textWidth = ImGui::CalcTextSize(entry->ElementName.c_str()).x;
                if (m_ThumbnailSize >= textWidth)
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + m_ThumbnailSize * 0.5f - textWidth * 0.5f);

                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + m_ThumbnailSize);
                ImGui::Text("%s", entry->ElementName.c_str());
                ImGui::PopTextWrapPos();
            }
            else // This file is being renamed
            {
                auto completeRename = [&]() {
                    if (m_RenamingPath.filename() == m_RenamingText)
                    {
                        m_RenamingPath.clear();
                        m_RenamingText.clear();
                        return;
                    }
                    Path newPath = EditorUtils::GetUniquePath(m_RenamingPath.parent_path() / m_RenamingText);

                    ProjectLibrary::Get().MoveEntry(m_RenamingPath, newPath);
                    UpdateDisplayList();
                    // TODO: This is inefficient
                    const Ref<LibraryEntry>& entry = ProjectLibrary::Get().FindEntry(newPath);
                    CW_ENGINE_ASSERT(entry);
                    if (entry) // Select the folder after it is renamed
                        m_SelectionSet.insert(entry->ElementNameHash);
                    // This doesn't work well with sort as the entries move around
                    m_SelectionEndIndex = m_SelectionStartIndex = entryIdx;

                    m_RenamingPath.clear();
                    m_RenamingText.clear();
                };
                ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 5));

                ImGui::SetKeyboardFocusHere();
                if (ImGui::InputText("##RenameFile", &m_RenamingText, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
                    completeRename();
                ImGui::PopStyleVar();

                if ((Input::IsMouseButtonDown(Mouse::ButtonLeft) || Input::IsMouseButtonDown(Mouse::ButtonRight)) && !ImGui::IsItemClicked())
                    completeRename();

                if (Input::IsKeyPressed(Key::Escape))
                    completeRename();
            }

            ImGui::EndGroup();

            // Selected card: 1px amber accent border around the card group.
            if (selected)
            {
                const ImVec2 cardMin = ImGui::GetItemRectMin();
                const ImVec2 cardMax = ImGui::GetItemRectMax();
                ImGui::GetWindowDrawList()->AddRect(cardMin, cardMax, UI::Colors::Accent, 3.0f, 0, 1.0f);
            }

            if (entryIdx == m_SelectionEndIndex)
                ImGui::ScrollToItem(ImGuiScrollFlags_KeepVisibleEdgeY);
            hovered |= ImGui::IsItemHovered();
            DrawAssetTooltip(path, preview);
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) // Allow dragging
            {
                UIUtils::SetAssetPayload(entry.get());
                ImGui::ImageButton("##thumb", tid, { m_ThumbnailSize, m_ThumbnailSize }, { 0, 1 }, { 1, 0 });
                ImGui::SetNextItemWidth(m_ThumbnailSize);
                float textWidth = ImGui::CalcTextSize(entry->ElementName.c_str()).x;
                if (m_ThumbnailSize >= textWidth)
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + m_ThumbnailSize * 0.5f - textWidth * 0.5f);

                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + m_ThumbnailSize);
                ImGui::Text("%s", entry->ElementName.c_str());
                ImGui::PopTextWrapPos();

                ImGui::EndDragDropSource();
            }

            if (entry->Type == LibraryEntryType::Directory) // Drop in directories
            {
                if (ImGui::BeginDragDropTarget())
                {
                    dropping = true;
                    if (const FileEntry* fileEntry = UIUtils::AcceptAssetPayload())
                    {
                        const Path& payloadPath = fileEntry->Filepath;
                        const Path filename = payloadPath.filename();
                        ProjectLibrary::Get().MoveEntry(payloadPath,
                                                        path / filename); // Perhaps I need to end here? Or rather I should change the display list
                        UpdateDisplayList();
                    }
                    ImGui::EndDragDropTarget();
                }
            }

            if (!selected)
                ImGui::PopStyleColor();
            ImGui::PopStyleColor();

            const bool shouldOpen = ImGui::IsItemHovered() && m_RenamingText.empty() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

            if (Input::IsMouseButtonUp(Mouse::ButtonLeft) || Input::IsMouseButtonUp(Mouse::ButtonRight))
            {
                if (ImGui::IsItemHovered() && !dropping) // TODO: Check if this is even necessary
                {
                    if (Input::IsKeyPressed(Key::LeftControl)) // Multi-select
                    {
                        if (selected)
                            m_SelectionSet.erase(entry->ElementNameHash);
                        else
                        {
                            if (m_SelectionSet.empty())
                                m_SelectionStartIndex = entryIdx;
                            m_SelectionSet.insert(entry->ElementNameHash);
                            m_SelectionEndIndex = entryIdx;
                        }
                    }
                    else if (Input::IsKeyPressed(Key::LeftShift) && m_SelectionStartIndex != (uint32_t)-1)
                    {
                        m_SelectionSet.clear();
                        if (entryIdx < m_SelectionStartIndex) // Select from right to left
                        {
                            for (uint32_t i = entryIdx; i <= m_SelectionStartIndex; i++)
                                m_SelectionSet.insert(displayList[i]->ElementNameHash);
                        }
                        else
                        {
                            for (uint32_t i = m_SelectionStartIndex; i <= entryIdx; i++)
                                m_SelectionSet.insert(displayList[i]->ElementNameHash);
                        }
                        m_SelectionEndIndex = entryIdx;
                    }
                    else
                    {
                        m_SelectionSet.clear();
                        m_SelectionSet.insert(entry->ElementNameHash);
                        m_SelectionEndIndex = m_SelectionStartIndex = entryIdx;
                        if (entry->Type != LibraryEntryType::Directory)
                            m_SetSelectedPathCallback(path);
                        else
                            m_SetSelectedPathCallback({});
                    }
                }
            }
            // TODO: Fix this with drag and drop. It will crash due to the MoveEntry call
            if (!dropping && ImGui::BeginPopupContextItem(entry->Filepath.string().c_str())) // Right click on a file
            {
                ShowContextMenuContents(entry.get());
                ImGui::EndPopup();
            }
            ImGui::PopID();
            if (shouldOpen)
            {
                ImGui::EndTable();
                HandleOpen(entry.get());
                return;
            }
        }

        if (Input::IsMouseButtonDown(Mouse::ButtonLeft) && !hovered && !ImGui::IsItemHovered() && ImGui::IsWindowHovered())
            ClearSelection();

        ImGui::EndTable();
    }

    void AssetBrowserPanel::DrawStatusBar() const
    {
        const size_t itemCount = m_DisplayList.size();
        if (!m_SearchString.empty())
            ImGui::TextDisabled("%zu search result%s", itemCount, itemCount == 1 ? "" : "s");
        else
            ImGui::TextDisabled("%zu item%s", itemCount, itemCount == 1 ? "" : "s");

        if (!m_SelectionSet.empty())
        {
            ImGui::SameLine();
            ImGui::TextDisabled("  %zu selected", m_SelectionSet.size());
        }

        if (m_AssetFilter != AssetBrowserFilter::All)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("  Filtered");
        }

        if (ProjectLibrary::Get().IsImporting())
        {
            const ImportProgress progress = ProjectLibrary::Get().GetImportProgress();
            const String status = fmt::format("Importing {} of {}", progress.CompletedFiles, progress.TotalFiles);
            const float statusWidth = ImGui::CalcTextSize(status.c_str()).x;
            ImGui::SameLine(std::max(ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x, ImGui::GetWindowContentRegionMax().x - statusWidth));
            ImGui::TextDisabled("%s", status.c_str());
        }
    }

    void AssetBrowserPanel::DrawTreeView()
    {
        ImGui::Begin("Tree view");

        std::function<void(const Ref<LibraryEntry>&, int32_t)> display = [&](const Ref<LibraryEntry>& cur, int32_t dirEntryIdx = -1) {
            if (cur->Type == LibraryEntryType::Directory)
            {
                DirectoryEntry* dirEntry = static_cast<DirectoryEntry*>(cur.get());

                // Need to check all children since we are only looking for directories and not files.
                bool hasChildren = false;
                for (const auto& child : dirEntry->Children)
                    if (child->Type == LibraryEntryType::Directory)
                        hasChildren = true;
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | (hasChildren ? 0 : ImGuiTreeNodeFlags_Leaf);

                if (m_CurrentDirectoryEntry->ElementNameHash == cur->ElementNameHash && m_CurrentDirectoryEntry->Filepath == cur->Filepath)
                    flags |= ImGuiTreeNodeFlags_Selected;

                if (dirEntryIdx != -1 && dirEntryIdx < m_DirectoryPathEntries.size() &&
                    cur->ElementNameHash == m_DirectoryPathEntries[dirEntryIdx]->ElementNameHash)
                {
                    ImGui::SetNextItemOpen(true);
                    dirEntryIdx++;
                }
                else
                    dirEntryIdx = -1;

                bool isOpen = ImGui::TreeNodeEx(cur->ElementName.c_str(), flags);

                if (!ImGui::IsItemToggledOpen() && ImGui::IsItemClicked())
                    SetCurrentDirectory(dirEntry);

                if (ImGui::BeginDragDropTarget())
                {
                    if (const FileEntry* fileEntry = UIUtils::AcceptAssetPayload())
                    {
                        // TODO: Make variant that takes in file entry too. Should be a bit faster.
                        ProjectLibrary::Get().MoveEntry(fileEntry->Filepath, cur->Filepath / fileEntry->Filepath.filename());
                        UpdateDisplayList();
                    }
                    ImGui::EndDragDropTarget();
                }
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) // Allow dragging
                {
                    UIUtils::SetAssetPayload(cur.get());
                    ImGui::EndDragDropSource();
                }

                if (ImGui::BeginPopupContextItem(cur->Filepath.string().c_str())) // Right click on a file
                {
                    ShowContextMenuContents(cur.get(), true);
                    ImGui::EndPopup();
                }

                if (Input::IsKeyUp(Key::Delete))
                {
                    // TODO: Need second selection set for the tree view
                }

                if (isOpen)
                {
                    for (const auto& child : dirEntry->Children)
                        display(child, dirEntryIdx);
                    ImGui::TreePop();
                }
            }
        };

        const Ref<DirectoryEntry>& root = ProjectLibrary::Get().GetRoot();
        // The m_LastCurrentDirectoryCheck is done for the ImGui::SetNextItemOpen later, without it it will try and open
        // every frame and we lose the ability to close the tree node.
        display(root, m_LastCurrentDirectory != m_CurrentDirectoryEntry->ElementNameHash ? 0 : -1);
        m_LastCurrentDirectory = m_CurrentDirectoryEntry->ElementNameHash;
        ImGui::End();
    }

    void AssetBrowserPanel::ShowContextMenuContents(LibraryEntry* entry, bool isTreeView)
    {
        if (ImGui::BeginMenu("Create"))
        {
            if (ImGui::MenuItem("Folder"))
            {
                if (isTreeView)
                {
                    CW_ENGINE_ASSERT(entry->Type == LibraryEntryType::Directory);
                    SetCurrentDirectory(static_cast<DirectoryEntry*>(entry));
                }
                CreateNew(AssetBrowserItem::Folder);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("C# script"))
                CreateNew(AssetBrowserItem::CScript);
            ImGui::Separator();
            if (ImGui::MenuItem("Scene"))
                CreateNew(AssetBrowserItem::Scene);
            if (ImGui::MenuItem("Prefab"))
                CreateNew(AssetBrowserItem::Prefab);
            if (ImGui::MenuItem("Node Graph"))
                CreateNew(AssetBrowserItem::NodeGraph);
            ImGui::Separator();
            if (ImGui::MenuItem("Material"))
                CreateNew(AssetBrowserItem::Material);
            if (ImGui::MenuItem("Shader"))
                CreateNew(AssetBrowserItem::Shader);
            if (ImGui::MenuItem("Compute Shader"))
                CreateNew(AssetBrowserItem::ComputeShader);
            if (ImGui::MenuItem("Render Texture"))
                CreateNew(AssetBrowserItem::RenderTexture);
            if (ImGui::MenuItem("Physics Material 2D"))
                CreateNew(AssetBrowserItem::PhysicsMaterial2D);
            if (ImGui::MenuItem("Physics Material 3D"))
                CreateNew(AssetBrowserItem::PhysicsMaterial3D);

            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Show In Explorer"))
        {
            if (entry == nullptr)
                PlatformUtils::ShowInExplorer(m_CurrentDirectoryEntry->Filepath);
            else
                PlatformUtils::ShowInExplorer(entry->Filepath);
        }

        if (entry == nullptr)
            ImGui::BeginDisabled();
        if (ImGui::MenuItem("Open"))
            HandleOpen(entry);

        if (ImGui::MenuItem("Delete"))
        {
            ProjectLibrary::Get().DeleteEntry(entry->Filepath);
            UpdateDisplayList();
        }

        if (ImGui::MenuItem("Rename"))
        {
            CW_ENGINE_ASSERT(entry != nullptr);
            if (entry)
                m_RenamingPath = entry->Filepath;
            m_RenamingText = m_RenamingPath.filename().string();
        }

        if (entry == nullptr)
            ImGui::EndDisabled();

        if (ImGui::MenuItem("Copy Path"))
        {
            if (entry != nullptr)
                PlatformUtils::CopyToClipboard(fs::absolute(entry->Filepath).string());
            else
                PlatformUtils::CopyToClipboard(fs::absolute(m_CurrentDirectoryEntry->Filepath).string());
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Refresh", "Ctrl+R"))
        {
            if (entry != nullptr)
                ProjectLibrary::Get().Refresh(entry->Filepath);
            else
            {
                ProjectLibrary::Get().Refresh(m_CurrentDirectoryEntry->Filepath);
                UpdateDisplayList();
            }
        }
    }

    void AssetBrowserPanel::CreateNew(AssetBrowserItem itemType)
    {
        // New assets must remain visible so rename can finish in place.
        m_SearchString.clear();
        m_AssetFilter = AssetBrowserFilter::All;
        const String filename = GetDefaultFileNameFromType(itemType);
        const Path newEntryPath = EditorUtils::GetUniquePath(m_CurrentDirectoryEntry->Filepath / filename);
        switch (itemType)
        {
        case AssetBrowserItem::Folder:
            ProjectLibrary::Get().CreateFolderEntry(newEntryPath);
            break;
        case AssetBrowserItem::CScript: {
            String text = GetDefaultContents(itemType);
            String className = newEntryPath.filename().replace_extension("").string();
            className = StringUtils::Replace(className, " ", "_");
            String script = StringUtils::Replace(text, "#NAMESPACE#", Editor::Get().GetProjectPath().filename().string());
            script = StringUtils::Replace(script, "#CLASSNAME#",
                                          className); // This has to be done after rename, since the file is saved first
                                                      // as NewScript and then as the user name.
            FileSystem::WriteTextFile(newEntryPath, script);
            break;
        }
        case AssetBrowserItem::PhysicsMaterial2D: {
            ProjectLibrary::Get().CreateEntry(CreateRef<PhysicsMaterial2D>(), newEntryPath);
            break;
        }
        case AssetBrowserItem::PhysicsMaterial3D: {
            ProjectLibrary::Get().CreateEntry(CreateRef<PhysicsMaterial3D>(), newEntryPath);
            break;
        }
        case AssetBrowserItem::Material: {
            AssetHandle<Shader> shader = AssetManager::TryGet()->Load<Shader>(UNLIT_SHADER_PATH);
            Ref<Material> material = Material::CreateUnlit(shader);
            ProjectLibrary::Get().CreateEntry(material, newEntryPath);
            break;
        }
        case AssetBrowserItem::NodeGraph: {
            Ref<NodeGraph> graph = CreateRef<NodeGraph>();
            graph->SetDomain(NodeGraph::Domain::Geometry);
            graph->SetName(newEntryPath.filename().replace_extension("").string());
            graph->AddNode(NodeRegistry::Get().Create("GeometryOutputNode"_sid));
            auto nodeGraphAsset = CreateRef<NodeGraphAsset>(graph);
            nodeGraphAsset->SetName(graph->GetName());
            ProjectLibrary::Get().CreateEntry(nodeGraphAsset, newEntryPath);
            break;
        }
        default: {
            FileSystem::WriteTextFile(newEntryPath, GetDefaultContents(itemType));
            break;
        }
        }
        ProjectLibrary::Get().Refresh(newEntryPath);
        const Ref<LibraryEntry> newEntry = ProjectLibrary::Get().FindEntry(newEntryPath);
        if (newEntry)
        {
            m_RenamingPath = newEntry->Filepath;
            m_RenamingText = newEntryPath.filename().string();
        }
        UpdateDisplayList();
    }

    void AssetBrowserPanel::RecalculateDirectoryEntries()
    {
        Path tmpPath;
        DirectoryEntry* tmp = m_CurrentDirectoryEntry;
        m_DirectoryPathEntries.clear();
        while (tmp != nullptr)
        {
            m_DirectoryPathEntries.push_back(tmp);
            tmp = tmp->Parent;
        }
        std::reverse(m_DirectoryPathEntries.begin(), m_DirectoryPathEntries.end());
    }

    String AssetBrowserPanel::GetDefaultContents(AssetBrowserItem itemType) const
    {
        switch (itemType)
        {
        case AssetBrowserItem::Material:
            return "# Crowny Material\\nShader: Default"; // Replace with uuid
        case AssetBrowserItem::CScript:
            return m_CsDefaultText;
        case AssetBrowserItem::Shader:
        case AssetBrowserItem::ComputeShader:
            return "# Crowny Shader"; // Need to decide on shader format
        case AssetBrowserItem::PhysicsMaterial2D:
        case AssetBrowserItem::PhysicsMaterial3D:
            return "# Crowny Physics Material\\n"; // Replace with uuid
        case AssetBrowserItem::Scene:
            return "# Crowny Scene\\nScene: Crowny scene\\nEntities:";
        case AssetBrowserItem::RenderTexture:
            return "# Crowny Render Target";
        default:
            return "";
        }
        CW_ENGINE_ASSERT(false);
        return "";
    }

} // namespace Crowny
