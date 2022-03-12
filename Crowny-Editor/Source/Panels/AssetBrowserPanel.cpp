#include "cwepch.h"

#include "Panels/AssetBrowserPanel.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/PlatformUtils.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/Input/Input.h"

#include "Editor/EditorAssets.h"
#include "Editor/EditorUtils.h"
#include "Editor/ProjectLibrary.h"

#include "Panels/UIUtils.h"

#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

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
            return "New Material.mat";
        case AssetBrowserItem::Prefab:
            return "New Prefab.prefab";
        case AssetBrowserItem::Shader:
            return "New Shader.shader";
        case AssetBrowserItem::ComputeShader:
            return "New ComputeShader.cshader";
        case AssetBrowserItem::PhysicsMaterial:
            return "New PhysicsMaterial.pmat";
        case AssetBrowserItem::RenderTexture:
            return "New RenderTexture.rt";
        case AssetBrowserItem::Scene:
            return "New Scene.cwscene";
        default:
            return "New File";
        }
    }
    
    AssetBrowserPanel::AssetBrowserPanel(const String& name,
                                                   std::function<void(const Path&)> selectedPathCallback)
      : ImGuiPanel(name), m_SetSelectedPathCallback(selectedPathCallback)
    {
        m_CsDefaultText = FileSystem::ReadTextFile(EditorAssets::DefaultScriptPath);
        m_FolderIcon = ImGui_ImplVulkan_AddTexture(EditorAssets::Get().FolderIcon);
        m_FileIcon = ImGui_ImplVulkan_AddTexture(EditorAssets::Get().FileIcon);
    }

    void AssetBrowserPanel::Initialize()
    {
        m_CurrentDirectoryEntry = ProjectLibrary::Get().GetRoot().get();

        for (auto child : m_CurrentDirectoryEntry->Children)
        {
            const auto& path = child->Filepath;
            String ext = path.extension().string();
            if (ext == ".png") // TODO: Replace the .png
            {
                Ref<Texture> result = Importer::Get().Import<Texture>(path);
                m_Textures[child->ElementNameHash] = result;
            }
        }
    }

    void AssetBrowserPanel::Render()
    {
        BeginPanel(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        DrawHeader();
        ImGui::Separator();

        ImGui::BeginChild("AssetBrowser", ImVec2(0, 0), false,
                          ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoNav);

        // Right click not on a file
        if (ImGui::BeginPopupContextWindow(nullptr,
                                           ImGuiPopupFlags_NoOpenOverExistingPopup | ImGuiPopupFlags_MouseButtonRight))
        {
            ShowContextMenuContents();
            ImGui::EndPopup();
        }

        // Files
        DrawFiles();

        ImGui::EndChild();
        EndPanel();
    }

    void AssetBrowserPanel::DrawHeader()
    {
        const Ref<DirectoryEntry>& entry = ProjectLibrary::Get().GetRoot();
        if (!m_BackwardHistory.empty())
        {
            if (ImGui::ArrowButton("<-", ImGuiDir_Left))
            {
                m_ForwardHistory.push(m_CurrentDirectoryEntry);
                m_CurrentDirectoryEntry = m_BackwardHistory.top();
                m_BackwardHistory.pop();
            }
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::ArrowButton("<-", ImGuiDir_Left);
            ImGui::EndDisabled();
        }
        ImGui::SameLine();
        if (!m_ForwardHistory.empty())
        {
            if (ImGui::ArrowButton("->", ImGuiDir_Right))
            {
                m_BackwardHistory.push(m_CurrentDirectoryEntry);
                m_CurrentDirectoryEntry = m_ForwardHistory.top();
                m_ForwardHistory.pop();
            }
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::ArrowButton("->", ImGuiDir_Right);
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if (ImGui::Button("Refresh"))
            ProjectLibrary::Get().Refresh(m_CurrentDirectoryEntry->Filepath);

        ImGui::SameLine();

        fs::path tmpPath;
        DirectoryEntry* tmp = m_CurrentDirectoryEntry;
        Vector<DirectoryEntry*> entries; // TODO: Do this once per directory operation
        while (tmp != nullptr)
        {
            entries.push_back(tmp);
            tmp = tmp->Parent;
        }
        std::reverse(entries.begin(), entries.end());
        for (auto* tmp : entries)
        {
            if (ImGui::Selectable(tmp->ElementName.c_str(), false, 0,
                                    ImVec2(ImGui::CalcTextSize(tmp->ElementName.c_str()).x, 0.0f)))
            {
                m_CurrentDirectoryEntry = tmp;
                break;
            }
            ImGui::SameLine();
            ImGui::Text("/");
            ImGui::SameLine();
        }

        /* scale slider */
        const float maxWidth = 150.0f * 1.1f;
        const float spacing = ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(" ").x;
        const float checkboxSize = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2;

        ImGui::SameLine(ImGui::GetWindowWidth() - maxWidth - 150.0f);

        ImGui::SetNextItemWidth(150.0f);
        float thumbnailChange = m_ThumbnailSize;
        if (ImGui::SliderFloat("##iconsize", &thumbnailChange, MIN_ASSET_THUMBNAIL_SIZE, MAX_ASSET_THUMBNAIL_SIZE))
        {
            m_Padding *= thumbnailChange / m_ThumbnailSize;
            m_ThumbnailSize = thumbnailChange;
            float cellSize = m_ThumbnailSize + m_Padding;
            float panelWidth = ImGui::GetContentRegionAvail().x;
            m_ColumnCount = (int)(panelWidth / cellSize);
            if (m_ColumnCount < 1)
                m_ColumnCount = 1;
        }

        /* File sorting modes */
        ImGui::SetNextItemWidth(150.0f);
        ImGui::SameLine(ImGui::GetContentRegionMax().x - 150.0f);
        static const char* sortingStr[3] = { "Sort By Name", "Sort By Size", "Sort By Date" };
        uint32_t currentMode = (uint32_t)m_FileSortingMode;
        if (ImGui::BeginCombo("##sort", sortingStr[currentMode]))
        {
            for (uint32_t i = 0; i < (uint32_t)FileSortingMode::SortCount; i++)
            {
                FileSortingMode mode = (FileSortingMode)i;
                if (ImGui::Selectable(sortingStr[i], i == currentMode))
                    m_FileSortingMode = mode;
            }
            ImGui::EndCombo();
        }

        if ((FileSortingMode)currentMode != m_FileSortingMode)
        {
            std::function<void(DirectoryEntry*)> sortChildren = [&](DirectoryEntry* dirEntry) {
                std::sort(dirEntry->Children.begin(), dirEntry->Children.end(), [&](const auto& l, const auto& r) {
                    if (m_FileSortingMode == FileSortingMode::SortByName)
                    {
                        if (l->Type == r->Type)
                            return l->ElementName < r->ElementName;
                        return l->Type == LibraryEntryType::File;
                    }
                    else if (m_FileSortingMode == FileSortingMode::SortByDate)
                        return l->LastUpdateTime < r->LastUpdateTime;
                    else if (m_FileSortingMode == FileSortingMode::SortBySize)
                    {
                        if (l->Type == r->Type && l->Type == LibraryEntryType::File)
                            return static_cast<FileEntry*>(l.get())->Filesize >
                                   static_cast<FileEntry*>(r.get())->Filesize;
                        return l->Type == LibraryEntryType::File;
                    }
                    return false;
                });
                for (auto& child : dirEntry->Children)
                {
                    if (child->Type == LibraryEntryType::Directory)
                        sortChildren(static_cast<DirectoryEntry*>(child.get()));
                }
            };

            sortChildren(ProjectLibrary::Get().GetRoot().get());
        }
    }

    void AssetBrowserPanel::HandleKeyboardNavigation()
    {
        if (Input::IsKeyUp(Key::Delete)) // Delete selected items
        {
            while (!m_SelectionSet.empty())
            {
                for (const auto& entry : m_CurrentDirectoryEntry->Children)
                {
                    auto iterFind = m_SelectionSet.find(entry->ElementNameHash);
                    if (iterFind != m_SelectionSet.end())
                    {
                        ProjectLibrary::Get().DeleteEntry(entry->Filepath);
                        m_SelectionSet.erase(iterFind);
                        break;
                    }
                }
            }
            m_SelectionStartIndex = 0;
            m_SelectionSet.clear();
        }

        if (Input::IsKeyUp(Key::F2)) // Rename the first selected item
        {
            if (m_SelectionStartIndex >= m_CurrentDirectoryEntry->Children.size())
                return;
            const Ref<LibraryEntry>& entry = m_CurrentDirectoryEntry->Children[m_SelectionStartIndex];
            if (!entry)
                return;
            m_RenamingPath = entry->Filepath; // TODO: Use hash instead of path
            m_RenamingText = m_RenamingPath.filename().string();
        }

        if (Input::IsKeyUp(Key::Enter)) // Enter a directory using the keyboard
        {
            if (m_CurrentDirectoryEntry->Type == LibraryEntryType::Directory)
            {
                m_BackwardHistory.push(m_CurrentDirectoryEntry);
                while (!m_ForwardHistory.empty())
                    m_ForwardHistory.pop();
                m_CurrentDirectoryEntry =
                    static_cast<DirectoryEntry*>(m_CurrentDirectoryEntry->Children[m_SelectionStartIndex].get());
            }
            else
                PlatformUtils::OpenExternally(m_CurrentDirectoryEntry->Children[m_SelectionStartIndex]->Filepath);
        }

        if (Input::IsKeyUp(Key::Backspace)) // Go back
        {
            if (m_CurrentDirectoryEntry->Parent != nullptr)
            {
                m_ForwardHistory.push(m_CurrentDirectoryEntry);
                while (!m_BackwardHistory.empty())
                    m_BackwardHistory.pop();
                m_CurrentDirectoryEntry = m_CurrentDirectoryEntry->Parent;
            }
        }

        if (Input::IsKeyPressed(Key::LeftControl))
        {
            if (Input::IsKeyUp(Key::C)) // Copy (Ctrl+C)
            {
                String clipboardString;
                for (const auto& entry : m_CurrentDirectoryEntry->Children)
                {
                    if (m_SelectionSet.find(entry->ElementNameHash) != m_SelectionSet.end())
                        clipboardString += entry->Filepath.string() + '\n';
                }
                clipboardString = clipboardString.substr(1, clipboardString.size() - 1);
                PlatformUtils::CopyToClipboard(clipboardString);
            }
            
            if (Input::IsKeyUp(Key::V)) // Paste (Ctrl+V)
            {
                String clipboard = PlatformUtils::CopyFromClipboard();
                Vector<String> paths = StringUtils::SplitString(clipboard, "\n");
                for (auto& path : paths) // Maybe here I would need to remove the last char
                    ProjectLibrary::Get().CopyEntry(path, EditorUtils::GetUniquePath(m_CurrentDirectoryEntry->Filepath / Path(path).filename()));
            }

            if (Input::IsKeyUp(Key::R)) // Refresh (Ctrl+R)
                ProjectLibrary::Get().Refresh(m_CurrentDirectoryEntry->Filepath);
        }

        // Keyboard navigation
        if (m_SelectionSet.empty()) // Select from unselected state
        {
            if (Input::IsKeyDown(Key::Left) || Input::IsKeyUp(Key::Up)) // Note: I can/should use imgui keys here.
            {
                if (m_CurrentDirectoryEntry->Children.size() > 0)
                {
                    m_SelectionSet.insert(
                        m_CurrentDirectoryEntry->Children[0]->ElementNameHash); // Select the first entry
                    m_SelectionStartIndex = 0;
                }
            }
            if (Input::IsKeyUp(Key::Right) || Input::IsKeyUp(Key::Down))
            {
                if (m_CurrentDirectoryEntry->Children.size() > 0)
                {
                    size_t lastIdx = m_CurrentDirectoryEntry->Children.size() - 1;
                    m_SelectionSet.insert(
                        m_CurrentDirectoryEntry->Children[lastIdx]->ElementNameHash); // Select the last entry
                    m_SelectionStartIndex = lastIdx;
                }
            }
        }
        else
        {
            if (Input::IsKeyDown(Key::Left))
            {
                m_SelectionSet.clear();
                m_SelectionStartIndex = std::max(0, (int32_t)m_SelectionStartIndex - 1);
                m_SelectionSet.insert(m_CurrentDirectoryEntry->Children[m_SelectionStartIndex]->ElementNameHash);
            }
            if (Input::IsKeyDown(Key::Right))
            {
                m_SelectionSet.clear();
                m_SelectionStartIndex =
                    std::min(m_SelectionStartIndex + 1, (uint32_t)m_CurrentDirectoryEntry->Children.size() - 1);
                m_SelectionSet.insert(m_CurrentDirectoryEntry->Children[m_SelectionStartIndex]->ElementNameHash);
            }
            if (Input::IsKeyUp(Key::Up))
            {
                m_SelectionSet.clear();
                m_SelectionStartIndex = std::max(0U, m_SelectionStartIndex - m_ColumnCount);
                m_SelectionSet.insert(m_CurrentDirectoryEntry->Children[m_SelectionStartIndex]->ElementNameHash);
            }
            if (Input::IsKeyUp(Key::Down))
            {
                m_SelectionSet.clear();
                m_SelectionStartIndex = std::min(m_SelectionStartIndex + m_ColumnCount,
                                                    (uint32_t)m_CurrentDirectoryEntry->Children.size() - 1);
                m_SelectionSet.insert(m_CurrentDirectoryEntry->Children[m_SelectionStartIndex]->ElementNameHash);
            }
        }
    }

    void AssetBrowserPanel::DrawFiles()
    {
        float cellSize = m_ThumbnailSize + m_Padding;
        float panelWidth = ImGui::GetContentRegionAvail().x;
        m_ColumnCount = (int)(panelWidth / cellSize);
        if (m_ColumnCount < 1)
            m_ColumnCount = 1;
        ImGui::Columns(m_ColumnCount, 0, false);

        bool dropping = false;

        if (m_CurrentDirectoryEntry == nullptr)
        {
            ImGui::Columns(1);
            return;
        }

        if (ImGui::IsWindowFocused() && (m_RenamingPath.empty() || m_RenamingPath != m_CurrentDirectoryEntry->Filepath))
            HandleKeyboardNavigation();

        // Files
        for (uint32_t entryIdx = 0; entryIdx < m_CurrentDirectoryEntry->Children.size(); entryIdx++)
        {
            const auto& entry = m_CurrentDirectoryEntry->Children[entryIdx];
            const auto& path = entry->Filepath;
            ImGui::PushID(entry->ElementNameHash);

            auto iterFind = m_SelectionSet.find(entry->ElementNameHash); // Show selected files
            bool selected = iterFind != m_SelectionSet.end();

            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            if (!selected)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImTextureID tid;
            if (entry->Type == LibraryEntryType::Directory)
                tid = m_FolderIcon;
            else
            {
                auto iter = m_Textures.find(entry->ElementNameHash);
                if (iter != m_Textures.end())
                    tid = ImGui_ImplVulkan_AddTexture(iter->second);
                else
                    tid = m_FileIcon;
            }

            // Thumbnail
            ImGui::BeginGroup();
            ImGui::ImageButton(tid, { m_ThumbnailSize, m_ThumbnailSize }, { 0, 1 }, { 1, 0 }, 0.0f);
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
                    CW_ENGINE_INFO("Rename: {0}, {1}", m_RenamingPath, m_RenamingPath.parent_path() / m_RenamingText);
                    ProjectLibrary::Get().MoveEntry(m_RenamingPath, m_RenamingPath.parent_path() / m_RenamingText);
                    m_RenamingPath.clear();
                };
                ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 5));

                ImGui::SetKeyboardFocusHere();
                if (ImGui::InputText("##RenameFile", &m_RenamingText,
                                     ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue |
                                       ImGuiInputTextFlags_CodeSelectNoExt))
                    completeRename();
                ImGui::PopStyleVar();

                if ((ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)) &&
                    !ImGui::IsItemClicked())
                    completeRename();
                if (ImGui::IsWindowFocused() && Input::IsKeyPressed(Key::Escape))
                    completeRename();

                ImGui::NextColumn();
            }

            ImGui::EndGroup();

            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) // Allow dragging
            {
                UIUtils::SetAssetPayload(path);
                ImGui::ImageButton(tid, { m_ThumbnailSize, m_ThumbnailSize }, { 0, 1 }, { 1, 0 }, 0.0f);
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
                    if (const ImGuiPayload* payload = UIUtils::AcceptAssetPayload())
                    {
                        Path payloadPath = UIUtils::GetPathFromPayload(payload).filename();
                        ProjectLibrary::Get().MoveEntry(path.parent_path() / payloadPath, path / payloadPath);
                    }
                    ImGui::EndDragDropTarget();
                }
            }

            if (!selected)
                ImGui::PopStyleColor();
            ImGui::PopStyleColor();

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) // Enter directory
            {
                if (entry->Type == LibraryEntryType::Directory)
                {
                    m_BackwardHistory.push(m_CurrentDirectoryEntry);
                    while (!m_ForwardHistory.empty())
                        m_ForwardHistory.pop();
                    m_CurrentDirectoryEntry = static_cast<DirectoryEntry*>(entry.get());
                    m_SelectionSet.clear();
                }
                else // Open the file
                {
                    // Note: Could directly open the file in the editor,
                    // Now I need to associate the file type with Crowny and also have some "only one app instance" rule
                    // and IPC
                    PlatformUtils::OpenExternally(path);
                }
            }

            if (ImGui::IsItemHovered() && !dropping && (ImGui::IsMouseReleased(ImGuiMouseButton_Left) ||
                                           ImGui::IsMouseReleased(ImGuiMouseButton_Right)))
            {
                if (Input::IsKeyPressed(Key::LeftControl)) // Multi-select
                {
                    if (selected)
                        m_SelectionSet.erase(entry->ElementNameHash);
                    else
                    {
                        if (m_SelectionSet.size() == 0)
                            m_SelectionStartIndex = entryIdx;
                        m_SelectionSet.insert(entry->ElementNameHash);
                    }
                }
                else // TODO: Shift select. Note: Need the first selected, if we have multiple files selected
                {
                    m_SelectionSet.clear();
                    m_SelectionSet.insert(entry->ElementNameHash);
                    m_SelectionStartIndex = entryIdx;
                    m_SetSelectedPathCallback(path);
                }
            }
            if (ImGui::BeginPopupContextItem(entry->Filepath.string().c_str())) // Right click on a file
            {
                ShowContextMenuContents(entry.get());
                ImGui::EndPopup();
            }
            ImGui::PopID();
            ImGui::NextColumn();
        }

        ImGui::Columns(1);
    }

    void AssetBrowserPanel::ShowContextMenuContents(LibraryEntry* entry)
    {
        if (ImGui::BeginMenu("Create"))
        {
            if (ImGui::MenuItem("Folder"))
                CreateNew(AssetBrowserItem::Folder);
            ImGui::Separator();
            if (ImGui::MenuItem("C# script"))
                CreateNew(AssetBrowserItem::CScript);
            ImGui::Separator();
            if (ImGui::MenuItem("Scene"))
                CreateNew(AssetBrowserItem::Scene);
            if (ImGui::MenuItem("Prefab"))
                CreateNew(AssetBrowserItem::Prefab);
            ImGui::Separator();
            if (ImGui::MenuItem("Material"))
                CreateNew(AssetBrowserItem::Material);
            if (ImGui::MenuItem("Shader"))
                CreateNew(AssetBrowserItem::Shader);
            if (ImGui::MenuItem("Compute Shader"))
                CreateNew(AssetBrowserItem::ComputeShader);
            if (ImGui::MenuItem("Render Texture"))
                CreateNew(AssetBrowserItem::RenderTexture);
            if (ImGui::MenuItem("Physics Material"))
                CreateNew(AssetBrowserItem::PhysicsMaterial);

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
            PlatformUtils::OpenExternally(entry->Filepath);

        if (ImGui::MenuItem("Delete"))
            ProjectLibrary::Get().DeleteEntry(entry->Filepath);

        if (ImGui::MenuItem("Rename"))
        {
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
            ProjectLibrary::Get().Refresh(m_CurrentDirectoryEntry->Filepath);
    }

    void AssetBrowserPanel::CreateNew(AssetBrowserItem itemType)
    {
        String filename = GetDefaultFileNameFromType(itemType);
        Path newEntryPath = EditorUtils::GetUniquePath(m_CurrentDirectoryEntry->Filepath / filename);
        switch (itemType)
        {
        case AssetBrowserItem::Folder:
            ProjectLibrary::Get().CreateFolderEntry(newEntryPath);
            break;
        default: {
            FileSystem::WriteTextFile(newEntryPath, GetDefaultContents(itemType));
            break;
        }
        }
        ProjectLibrary::Get().Refresh(newEntryPath);
        m_RenamingPath = ProjectLibrary::Get().FindEntry(newEntryPath)->Filepath;
        m_RenamingText = newEntryPath.filename().string();
    }

    String AssetBrowserPanel::GetDefaultContents(AssetBrowserItem itemType)
    {
        switch (itemType)
        {
        case AssetBrowserItem::Material:
            return "# Crowny Material\\nShader: Default"; // Replace with uuid
        case AssetBrowserItem::CScript:
            return m_CsDefaultText; // TODO: Replace file name and namespace
        case AssetBrowserItem::Shader:
        case AssetBrowserItem::ComputeShader:
            return "# Crowny Shader"; // Need to decide on shader format
        case AssetBrowserItem::PhysicsMaterial:
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