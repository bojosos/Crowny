#include "cwpch.h"

#include "Panels/ImGuiAssetBrowserPanel.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/PlatformUtils.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/Input/Input.h"

#include "Editor/ProjectLibrary.h"

#include "Editor/EditorAssets.h"
#include "Editor/EditorUtils.h"

#include <backends/imgui_impl_vulkan.h>
#include <imgui_internal.h>

#include <ctime>
#include <filesystem>
#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

namespace Crowny
{

    static const fs::path s_AssetPath = "Resources";

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

    struct FileSortingEntry
    {
        FileSortingEntry(long key, const fs::directory_entry& entry) : Key(key), Entry(entry) {}

        long Key;
        fs::directory_entry Entry;
    };

    ImGuiAssetBrowserPanel::ImGuiAssetBrowserPanel(const String& name,
                                                   std::function<void(const Path&)> selectedPathCallback)
      : ImGuiPanel(name), m_SetSelectedPathCallback(selectedPathCallback)
    {
        m_CsDefaultText = FileSystem::ReadTextFile(EditorAssets::DefaultScriptPath);
        m_FolderIcon = ImGui_ImplVulkan_AddTexture(EditorAssets::Get().FolderIcon);
        m_FileIcon = ImGui_ImplVulkan_AddTexture(EditorAssets::Get().FileIcon);
    }

    void ImGuiAssetBrowserPanel::Initialize()
    {
        m_CurrentDirectoryEntry = ProjectLibrary::Get().GetRoot().get();

        for (auto child : m_CurrentDirectoryEntry->Children)
        {
            const auto& path = child->Filepath;
            String ext = path.extension();
            if (ext == ".png") // TODO: Replace the .png
            {
                Ref<Texture> result = Importer::Get().Import<Texture>(path);
                m_Textures[child->ElementName] = result;
            }
        }
    }

    void ImGuiAssetBrowserPanel::Render()
    {
        UpdateState();
        ImGui::Begin("Asset browser", &m_Shown, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        DrawHeader();
        ImGui::Separator();

        ImGui::BeginChild("AssetBrowser", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
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

        ImGui::End();
    }

    void ImGuiAssetBrowserPanel::DrawHeader()
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
            ImGui::PushDisabled();
            ImGui::ArrowButton("<-", ImGuiDir_Left);
            ImGui::PopDisabled();
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
            ImGui::PushDisabled();
            ImGui::ArrowButton("->", ImGuiDir_Right);
            ImGui::PopDisabled();
        }

        ImGui::SameLine();
        if (ImGui::Button("Refresh"))
            ProjectLibrary::Get().Refresh(m_CurrentDirectoryEntry->Filepath);

        ImGui::SameLine();

        fs::path tmpPath;
        DirectoryEntry* tmp = m_CurrentDirectoryEntry;
        while (tmp != nullptr)
        {
            tmpPath /= tmp->Filepath.filename();
            if (ImGui::Selectable(tmp->ElementName.c_str(), false, 0,
                                  ImVec2(ImGui::CalcTextSize(tmp->ElementName.c_str()).x, 0.0f)))
            {
                m_CurrentDirectoryEntry = tmp;
                break;
            }
            ImGui::SameLine();
            ImGui::Text("/");
            ImGui::SameLine();
            tmp = tmp->Parent;
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
        }
        /* file sorting */
        ImGui::SetNextItemWidth(150.0f);
        ImGui::SameLine(ImGui::GetContentRegionMax().x - 150.0f);
        static const char* sortingStr[4] = { "Sort By Name", "Sort By Size", "Sort By Date", "Sort By Type" };
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
    }

    void ImGuiAssetBrowserPanel::DrawFiles()
    {
        float cellSize = m_ThumbnailSize + m_Padding;
        float panelWidth = ImGui::GetContentRegionAvail().x;
        int columnCount = (int)(panelWidth / cellSize);
        if (columnCount < 1)
            columnCount = 1;
        ImGui::Columns(columnCount, 0, false);

        if (m_CurrentDirectoryEntry == nullptr)
        {
            ImGui::Columns(1);
            return;
        }

        Vector<Ref<LibraryEntry>> entries = m_CurrentDirectoryEntry->Children;
        std::sort(entries.begin(), entries.end(), [&](const auto& l, const auto& r) {
            if (m_FileSortingMode == FileSortingMode::SortByName)
            {
                if (l->Type == r->Type)
                    return l->ElementName > r->ElementName;
                return l->Type == LibraryEntryType::File;
            }
            else if (m_FileSortingMode == FileSortingMode::SortByDate)
                return l->LastUpdateTime > r->LastUpdateTime;
            else if (m_FileSortingMode == FileSortingMode::SortBySize)
            {
                if (l->Type == r->Type && l->Type == LibraryEntryType::File)
                    return static_cast<FileEntry*>(l.get())->Filesize > static_cast<FileEntry*>(r.get())->Filesize;
                return l->Type == LibraryEntryType::File;
            }
            return false;
        });

        if (m_Focused || m_Hovered)
        {
            if (Input::IsKeyPressed(Key::Delete))
            {
                for (auto& entry : m_SelectedFiles)
                {
                    if (!fs::remove((s_AssetPath / entry).c_str()))
                        CW_ENGINE_ERROR("Error deleting file.");
                }
            }
            if (Input::IsKeyPressed(Key::F2))
            {
                m_RenamingPath = *m_SelectedFiles.begin(); // TODO: Make sure this is the first selected file
                m_Filename = m_RenamingPath.filename();
            }
            // if (Input::IsKeyPressed(Key::Left) || Input::IsKeyPressed(Key::Right))
            //     m_SelectedFiles.insert(sortedFiles[0].Entry.path().filename().string());
        }

        for (const auto& entry : entries)
        {
            const auto& path = entry->Filepath;
            auto relativePath = fs::relative(path, ProjectLibrary::Get().GetAssetFolder());
            String filename = relativePath.filename().string();
            ImGui::PushID(filename.c_str());

            auto iterFind = m_SelectedFiles.find(filename); // Show selected files
            bool selected = iterFind != m_SelectedFiles.end();

            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            if (!selected)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImTextureID tid;
            if (entry->Type == LibraryEntryType::Directory)
                tid = m_FolderIcon;
            else
            {
                auto iter = m_Textures.find(filename);
                if (iter != m_Textures.end())
                    tid = ImGui_ImplVulkan_AddTexture(iter->second);
                else
                    tid = m_FileIcon;
            }

            // The thumbnail
            ImGui::BeginGroup();
            ImGui::ImageButton(tid, { m_ThumbnailSize, m_ThumbnailSize }, { 0, 1 }, { 1, 0 }, 0.0f);
            ImGui::SetNextItemWidth(m_ThumbnailSize);
            if (m_RenamingPath.empty() || m_RenamingPath != path)
            {
                float textWidth = ImGui::CalcTextSize(entry->ElementName.c_str()).x;
                if (m_ThumbnailSize >= textWidth)
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + m_ThumbnailSize / 2 - textWidth / 2);

                ImGui::GetCurrentWindow()->DrawList->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                                                             IM_COL32(255, 255, 0, 255));

                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + m_ThumbnailSize);
                ImGui::Text("%s", entry->ElementName.c_str());
                ImGui::PopTextWrapPos();
            }
            else
            {
                auto completeRename = [&]() {
                    ProjectLibrary::Get().MoveEntry(m_RenamingPath, m_RenamingPath.parent_path() / m_Filename);
                    m_RenamingPath.clear();
                };
                ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 5));

                ImGui::SetKeyboardFocusHere();
                if (ImGui::InputText("##RenameFile", &m_Filename,
                                     ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue |
                                       ImGuiInputTextFlags_CodeSelectNoExt))
                    completeRename();
                ImGui::PopStyleVar();

                if ((ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)) &&
                    !ImGui::IsItemClicked())
                    completeRename();
                if (m_Focused && Input::IsKeyPressed(Key::Escape))
                    completeRename();

                ImGui::NextColumn();
            }

            ImGui::EndGroup();

            // Allow dragging
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                const char* itemPath = path.c_str();
                ImGui::SetDragDropPayload("ASSET_ITEM", itemPath, strlen(itemPath) * sizeof(char));
                ImGui::ImageButton(tid, { m_ThumbnailSize, m_ThumbnailSize }, { 0, 1 }, { 1, 0 }, 0.0f);
                ImGui::SetNextItemWidth(m_ThumbnailSize);
                float textWidth = ImGui::CalcTextSize(entry->ElementName.c_str()).x;
                if (m_ThumbnailSize >= textWidth)
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + m_ThumbnailSize / 2 - textWidth / 2);

                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + m_ThumbnailSize);
                ImGui::Text("%s", entry->ElementName.c_str());
                ImGui::PopTextWrapPos();

                ImGui::EndDragDropSource();
            }

            if (entry->Type == LibraryEntryType::Directory) // Drop in directories
            {
                if (ImGui::BeginDragDropTarget())
                {
                    const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_ITEM");
                    if (payload)
                    {
                        fs::path payloadPath = fs::path((const char*)payload->Data).filename();
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
                    m_SelectedFiles.clear();
                }
                else // Open the file
                {
                    // Note: Could directly open the file in the editor,
                    // Now I need to associate the file type with Crowny and also have some "only one app instance" rule
                    // and IPC
                    PlatformUtils::OpenExternally(path);
                }
            }

            if (ImGui::IsItemHovered() && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
                                           ImGui::IsMouseClicked(ImGuiMouseButton_Right))) // Multi-select
            {
                if (Input::IsKeyPressed(Key::LeftControl))
                {
                    if (selected)
                        m_SelectedFiles.erase(iterFind);
                    else
                        m_SelectedFiles.insert(filename);
                }
                else // TODO: Shift select. Note: Need the first selected, if we have multiple files selected
                {
                    m_SelectedFiles.clear();
                    m_SelectedFiles.insert(filename);
                    m_SetSelectedPathCallback(path);
                }
            }
            if (ImGui::BeginPopupContextItem(filename.c_str())) // Right click on a file
            {
                ShowContextMenuContents(path.string());
                ImGui::EndPopup();
            }
            ImGui::PopID();
            ImGui::NextColumn();
        }

        ImGui::Columns(1);
    }

    void ImGuiAssetBrowserPanel::ShowContextMenuContents(const Path& filepath)
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
            if (filepath.empty())
                PlatformUtils::ShowInExplorer(m_CurrentDirectoryEntry->Filepath);
            else
                PlatformUtils::ShowInExplorer(filepath);
        }

        if (filepath.empty())
            ImGui::PushDisabled();
        if (ImGui::MenuItem("Open"))
        {
            PlatformUtils::OpenExternally(filepath);
        }

        if (ImGui::MenuItem("Delete"))
        {
            ProjectLibrary::Get().DeleteEntry(filepath);
        }

        if (ImGui::MenuItem("Rename"))
        {
            m_RenamingPath = filepath;
            m_Filename = m_RenamingPath.filename();
        }

        if (filepath.empty())
            ImGui::PopDisabled();

        if (ImGui::MenuItem("Copy Path"))
        {
            if (!filepath.empty())
                PlatformUtils::CopyToClipboard(fs::absolute(filepath));
            else
                PlatformUtils::CopyToClipboard(fs::absolute(m_CurrentDirectoryEntry->Filepath));
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Refresh", "Ctrl+R"))
            ProjectLibrary::Get().Refresh(m_CurrentDirectoryEntry->Filepath);
    }

    void ImGuiAssetBrowserPanel::CreateNew(AssetBrowserItem itemType)
    {
        String filename = GetDefaultFileNameFromType(itemType);
        Path newEntryPath = EditorUtils::GetUniquePath(m_CurrentDirectoryEntry->Filepath / filename);
        CW_ENGINE_INFO(newEntryPath);
        switch (itemType)
        {
        case AssetBrowserItem::Folder:
            ProjectLibrary::Get().CreateFolderEntry(newEntryPath);
            break;
        default: {
            FileSystem::WriteTextFile(newEntryPath, GetDefaultContents(itemType));
            ProjectLibrary::Get().Refresh(newEntryPath);
            break;
        }
        }
        ProjectLibrary::Get().Refresh(newEntryPath);
        m_RenamingPath = newEntryPath;
        m_Filename = newEntryPath.filename();
    }

    String ImGuiAssetBrowserPanel::GetDefaultContents(AssetBrowserItem itemType)
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

    void ImGuiAssetBrowserPanel::Show() { m_Shown = true; }

    void ImGuiAssetBrowserPanel::Hide() { m_Shown = false; }

} // namespace Crowny