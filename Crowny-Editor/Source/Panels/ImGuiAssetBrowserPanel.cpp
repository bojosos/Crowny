#include "cwpch.h"

#include "Panels/ImGuiAssetBrowserPanel.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/PlatformUtils.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Input/Input.h"
#include "Editor/EditorAssets.h"

#include <backends/imgui_impl_vulkan.h>
#include <imgui_internal.h>

#include <ctime>
#include <filesystem>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

namespace Crowny
{

    static const std::filesystem::path s_AssetPath = "Resources";
    extern void LoadTexture(const std::string& filepath, Ref<Texture>& texture);

    // https://stackoverflow.com/questions/61030383/how-to-convert-stdfilesystemfile-time-type-to-time-t
    template <typename TP> static time_t ToCTime(TP tp)
    {
        using namespace std::chrono;
        auto sctp = time_point_cast<system_clock::duration>(tp - TP::clock::now() + system_clock::now());
        return system_clock::to_time_t(sctp);
    }

    static std::string GetDefaultFileNameFromType(AssetBrowserItem type)
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
        FileSortingEntry(long key, const std::filesystem::directory_entry& entry) : Key(key), Entry(entry) {}

        long Key;
        std::filesystem::directory_entry Entry;
    };

    ImGuiAssetBrowserPanel::ImGuiAssetBrowserPanel(const std::string& name)
      : ImGuiPanel(name), m_CurrentDirectory(s_AssetPath)
    {
        m_FolderIcon = ImGui_ImplVulkan_AddTexture(EditorAssets::Get().FolderIcon);
        m_FileIcon = ImGui_ImplVulkan_AddTexture(EditorAssets::Get().FileIcon);

        for (auto& dir : std::filesystem::recursive_directory_iterator(s_AssetPath))
        {
            const auto& path = dir.path();
            std::string filename = path.filename().string();
            if (StringUtils::EndWith(filename, ".png"))
            {
                Ref<Texture> result;
                // LoadTexture(path, result);
                m_Textures[filename] = result;
            }
        }

        m_CsDefaultText = FileSystem::ReadTextFile(EditorAssets::DefaultScriptPath);
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
        if (m_CurrentDirectory != std::filesystem::path(s_AssetPath))
        {
            if (ImGui::ArrowButton("<-", ImGuiDir_Left))
            {
                m_PreviousDirectory = m_CurrentDirectory;
                m_CurrentDirectory = m_CurrentDirectory.parent_path();
            }
            ImGui::SameLine();

            if (m_PreviousDirectory.empty())
            {
                ImGui::PushDisabled();
                ImGui::ArrowButton("->", ImGuiDir_Right);
                ImGui::PopDisabled();
            }
            else if (ImGui::ArrowButton("->", ImGuiDir_Right))
                m_CurrentDirectory = m_PreviousDirectory;
        }
        else
        {
            ImGui::PushDisabled();
            ImGui::ArrowButton("<-", ImGuiDir_Left);
            ImGui::PopDisabled();
            ImGui::SameLine();

            if (m_PreviousDirectory.empty())
            {
                ImGui::PushDisabled();
                ImGui::ArrowButton("->", ImGuiDir_Right);
                ImGui::PopDisabled();
            }
            else if (ImGui::ArrowButton("->", ImGuiDir_Right))
                m_CurrentDirectory = m_PreviousDirectory;
        }

        ImGui::SameLine();
        if (ImGui::Button("Refresh"))
        {
        }
        ImGui::SameLine();

        std::filesystem::path tmpPath;
        for (auto& dir : m_CurrentDirectory)
        {
            tmpPath /= dir;
            if (ImGui::Selectable(dir.c_str(), false, 0, ImVec2(ImGui::CalcTextSize(dir.c_str(), NULL, true).x, 0.0f)))
            {
                m_CurrentDirectory = tmpPath;
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

        std::vector<FileSortingEntry> sortedFiles; // assumes time_t is long
        if (m_FileSortingMode == FileSortingMode::SortByName)
        {
            std::set<std::filesystem::directory_entry> sortedDirs;
            for (auto& dir : std::filesystem::directory_iterator(m_CurrentDirectory))
            {
                sortedDirs.insert(dir);
            }

            long id = 0;
            for (auto& dir : sortedDirs)
            {
                sortedFiles.emplace_back(id++, dir);
            }
        }
        else
        {
            std::set<std::filesystem::directory_entry> folders;
            for (auto& dir : std::filesystem::directory_iterator(m_CurrentDirectory))
            {
                if (m_FileSortingMode == FileSortingMode::SortBySize)
                {
                    if (dir.is_regular_file())
                    {
                        long id = dir.file_size();
                        sortedFiles.emplace_back(id, dir);
                    }
                    else if (dir.is_directory())
                        folders.insert(dir);
                }
                if (m_FileSortingMode == FileSortingMode::SortByDate)
                {
                    if (dir.is_regular_file())
                    {
                        long id = ToCTime(dir.last_write_time());
                        sortedFiles.emplace_back(id, dir);
                    }
                    else if (dir.is_directory())
                        folders.insert(dir);
                }
            }

            std::sort(sortedFiles.begin(), sortedFiles.end(),
                      [](const FileSortingEntry& a, const FileSortingEntry& b) { // lambda ception
                          if (a.Key != b.Key)
                              return a.Key > b.Key;
                          std::string aStr = a.Entry.path().filename().string();
                          std::string bStr = b.Entry.path().filename().string();
                          return std::lexicographical_compare(
                            aStr.begin(), aStr.end(), bStr.begin(), bStr.end(),
                            [](char a, char b) { return std::tolower(a) < std::tolower(b); });
                      });

            for (auto& folder : folders)
            {
                sortedFiles.emplace_back(0, folder);
            }
        }

        if (m_Focused || m_Hovered)
        {
            if (Input::IsKeyPressed(Key::Delete))
            {
                for (auto& entry : m_SelectedFiles)
                {
                    if (!std::filesystem::remove((s_AssetPath / entry).c_str()))
                        CW_ENGINE_ERROR("Error deleting file.");
                }
            }
            if (Input::IsKeyPressed(Key::F2))
            {
                m_RenamingPath = *m_SelectedFiles.begin(); // TODO: Make sure this is the first file
                m_Filename = m_RenamingPath.filename();
            }
            if (Input::IsKeyPressed(Key::Left) || Input::IsKeyPressed(Key::Right))
            {
                CW_ENGINE_INFO(sortedFiles[0].Entry.path().filename().string());
                m_SelectedFiles.insert(sortedFiles[0].Entry.path().filename().string());
            }
        }

        for (auto& entry : sortedFiles)
        {
            std::filesystem::directory_entry dir = entry.Entry;
            const auto& path = dir.path();
            auto relativePath = std::filesystem::relative(path, s_AssetPath);
            std::string filename = relativePath.filename().string();
            ImGui::PushID(filename.c_str());
            auto iterFind = m_SelectedFiles.find(filename); // Show selected files
            bool selected = iterFind != m_SelectedFiles.end();
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            if (selected)
            {
            }
            else
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImTextureID tid;
            if (dir.is_directory())
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
            ImGui::ImageButton(tid, { m_ThumbnailSize, m_ThumbnailSize }, { 0, 1 },
                               { 1, 0 } /*, -1, ImGui::GetStyleColorVec4(ImGuiCol_Header)*/);

            if (m_RenamingPath.empty() || m_RenamingPath != path)
                ImGui::TextWrapped("%s", filename.c_str());
            else
            {
                auto completeRename = [&]() {
                    if (m_RenamingType == AssetBrowserItem::Folder)
                        std::filesystem::rename(m_RenamingPath,
                                                m_CurrentDirectory / m_Filename); // Hmm does not feel safe
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

                ImGui::NextColumn();
            }

            ImGui::EndGroup();

            // Allow dragging
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                const char* itemPath = path.c_str();
                ImGui::SetDragDropPayload("ASSET_ITEM", itemPath, strlen(itemPath) * sizeof(char));
                ImGui::ImageButton(tid, { m_ThumbnailSize, m_ThumbnailSize }, { 0, 1 },
                                   { 1, 0 } /*, -1, ImGui::GetStyleColorVec4(ImGuiCol_Header)*/);
                ImGui::TextWrapped("%s", filename.c_str());

                ImGui::EndDragDropSource();
            }

            if (dir.is_directory()) // Drop in directoriesy
            {
                if (ImGui::BeginDragDropTarget())
                {
                    const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_ITEM");
                    if (payload)
                    {
                        std::filesystem::path payloadPath =
                          std::filesystem::path((const char*)payload->Data).filename();
                        std::filesystem::rename(path.parent_path() / payloadPath, path / payloadPath);
                    }
                    ImGui::EndDragDropTarget();
                }
            }

            if (!selected)
                ImGui::PopStyleColor();
            ImGui::PopStyleColor();

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) // Enter directory
            {
                if (dir.is_directory())
                {
                    m_PreviousDirectory.clear();
                    m_CurrentDirectory /= path.filename();
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

    void ImGuiAssetBrowserPanel::ShowContextMenuContents(const std::string& filepath)
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
                PlatformUtils::ShowInExplorer(m_CurrentDirectory);
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
            if (!std::filesystem::remove((filepath).c_str()))
                CW_ENGINE_ERROR("Error deleting file.");
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
                PlatformUtils::CopyToClipboard(std::filesystem::absolute(filepath));
            else
                PlatformUtils::CopyToClipboard(std::filesystem::absolute(m_CurrentDirectory));
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Refresh", "Ctrl+R"))
        {
        }
    }

    void ImGuiAssetBrowserPanel::CreateNew(AssetBrowserItem itemType)
    {
        std::string filename = GetDefaultFileNameFromType(itemType);
        if (itemType == AssetBrowserItem::Folder)
        {
            if (!std::filesystem::create_directory(m_CurrentDirectory / filename))
                CW_ENGINE_ERROR("Error creating directory {0}", (m_CurrentDirectory / filename).string());
        }
        else
        {
            const std::string text = GetDefaultContents(itemType);
            FileSystem::WriteTextFile(m_CurrentDirectory / filename, text);
        }
        m_RenamingPath = m_CurrentDirectory / filename;
        m_Filename = filename;
    }

    std::string ImGuiAssetBrowserPanel::GetDefaultContents(AssetBrowserItem itemType)
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