#include "cwepch.h"

#include "Panels/AssetBrowserPanel.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/PlatformUtils.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/Input/Input.h"

#include "Editor/Editor.h"
#include "Editor/EditorAssets.h"
#include "Editor/EditorUtils.h"
#include "Editor/ProjectLibrary.h"

#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/RenderAPI/RenderTexture.h"

#include "UI/UIUtils.h"

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

    AssetBrowserPanel::AssetBrowserPanel(const String& name, std::function<void(const Path&)> selectedPathCallback)
      : ImGuiPanel(name), m_SetSelectedPathCallback(selectedPathCallback)
    {
        m_CsDefaultText = FileSystem::ReadTextFile(EditorAssets::DefaultScriptPath);
        m_FolderIcon = ImGui_ImplVulkan_AddTexture(EditorAssets::Get().FolderIcon);
        m_FileIcon = ImGui_ImplVulkan_AddTexture(EditorAssets::Get().FileIcon);
    }

    void AssetBrowserPanel::Initialize()
    {
        if (Editor::Get().GetProjectSettings()->LastAssetBrowserSelectedEntry.empty())
            m_CurrentDirectoryEntry = ProjectLibrary::Get().GetRoot().get();
        else
        {
            LibraryEntry* entry =
              ProjectLibrary::Get().FindEntry(Editor::Get().GetProjectSettings()->LastAssetBrowserSelectedEntry).get();
            if (entry == nullptr || entry->Type == LibraryEntryType::File)
                m_CurrentDirectoryEntry = ProjectLibrary::Get().GetRoot().get();
            else
                m_CurrentDirectoryEntry = static_cast<DirectoryEntry*>(entry);
        }
        RecalculateDirectoryEntries();

        TextureParameters soundWaveParams;
        soundWaveParams.Width = 256;
        soundWaveParams.Height = 256;
        soundWaveParams.Usage = TextureUsage::TEXTURE_STATIC;
        soundWaveParams.Format = TextureFormat::RGBA8;

        Ref<Texture> soundWave = Texture::Create(soundWaveParams);

        for (auto child : m_CurrentDirectoryEntry->Children) // Also this is not recursive, and do audio wave on import
        {
            const auto& path = child->Filepath;
            Renderer2D::Begin(glm::ortho(-256.0f, 256.0f, -256.0f, 256.0f), glm::mat4(1.0f));
            String ext = path.extension().string();
            if (ext == ".png") // TODO: Replace the .png
            {
                Ref<Texture> result = Importer::Get().Import<Texture>(path);
                m_Icons[child->ElementNameHash] = result;
            }
            else if (ext == ".ogg")
            {
                AssetHandle<AudioClip> clip = static_asset_cast<AudioClip>(ProjectLibrary::Get().Load(path));
                Vector<uint8_t> samples;
                samples.resize(clip->GetNumSamples() * 2);
                clip->GetBuffer(samples.data(), 0, (uint32_t)samples.size());
                struct icol
                {
                    uint8_t r, g, b, a;
                };
                icol data[256][256];
                std::memset(data, 0, 256 * 256 * 4);
                for (uint32_t c = 0; c < clip->GetNumChannels(); c++)
                {
                    float x = 0, xAdv = 256.0f / samples.size() * 2;
                    for (uint32_t i = 0; i < samples.size(); i += 2)
                    {
                        int16_t sample = (((int(samples[i]))) | (int(samples[i + 1]) << 8));
                        data[256 - int((float)sample / 65535 * 256) - 256 / 2]
                            [int(x) /* clip->GetNumChannels() * (c + 1))*/] = { 39, 185, 242, 255 };
                        x += xAdv;
                    }
                }
                PixelData src(256, 256, 1, TextureFormat::RGBA8);
                src.SetBuffer((uint8_t*)data);
                soundWave->WriteData(src);
                m_Icons[child->ElementNameHash] = soundWave;
            }
        }
        UpdateDisplayList();
    }

    void AssetBrowserPanel::Render()
    {
        UI::ScopedStyle windowPadding(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 2.0f));
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
        DrawTreeView();
    }

    void AssetBrowserPanel::SetCurrentDirectory(DirectoryEntry* entry)
    {
        m_ForwardHistory = {};
        m_BackwardHistory.push(m_CurrentDirectoryEntry);

        m_CurrentDirectoryEntry = entry;
        RecalculateDirectoryEntries();
        m_SelectionSet.clear();
        m_SelectionStartIndex = (uint32_t)-1;

        m_RenamingPath.clear();
        m_RenamingText.clear();
        m_RequiresSort = true;
        UpdateDisplayList();
    }

    void AssetBrowserPanel::GoBackward()
    {
        if (m_BackwardHistory.empty())
            return;

        m_ForwardHistory.push(m_CurrentDirectoryEntry);
        m_CurrentDirectoryEntry = m_BackwardHistory.top();
        RecalculateDirectoryEntries();
        m_BackwardHistory.pop();

        m_RenamingPath.clear();
        m_RenamingText.clear();
        m_RequiresSort = true;
        UpdateDisplayList();
    }

    void AssetBrowserPanel::GoForward()
    {
        if (m_ForwardHistory.empty())
            return;

        m_BackwardHistory.push(m_CurrentDirectoryEntry);
        m_CurrentDirectoryEntry = m_ForwardHistory.top();
        RecalculateDirectoryEntries();
        m_ForwardHistory.pop();

        m_RenamingPath.clear();
        m_RenamingText.clear();
        m_RequiresSort = true;
        UpdateDisplayList();
    }

    void AssetBrowserPanel::DrawHeader()
    {
        // UI::ScopedStyle itemSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 3));
        UI::ScopedStyle style2(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
        UI::ScopedStyle style3(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
        ImGui::BeginVertical("##assetBrowserV", { ImGui::GetContentRegionAvailWidth(), 0 }, 0.5f);
        ImGui::Spring();
        ImGui::BeginHorizontal("##assetBrowserH", { ImGui::GetContentRegionAvailWidth(), 0 });
        const Ref<DirectoryEntry>& entry = ProjectLibrary::Get().GetRoot();
        if (!m_BackwardHistory.empty())
        {
            if (ImGui::ArrowButton("<-", ImGuiDir_Left))
                GoBackward();
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::ArrowButton("<-", ImGuiDir_Left);
            ImGui::EndDisabled();
        }

        if (!m_ForwardHistory.empty())
        {
            if (ImGui::ArrowButton("->", ImGuiDir_Right))
                GoForward();
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::ArrowButton("->", ImGuiDir_Right);
            ImGui::EndDisabled();
        }

        if (ImGui::Button("Refresh"))
        {
            ProjectLibrary::Get().Refresh(m_CurrentDirectoryEntry->Filepath);
            UpdateDisplayList();
        }

        for (DirectoryEntry* dirEntry : m_DirectoryPathEntries)
        {
            if (ImGui::Selectable(dirEntry->ElementName.c_str(), false, 0,
                                  ImVec2(ImGui::CalcTextSize(dirEntry->ElementName.c_str()).x, 0.0f)))
            {
                SetCurrentDirectory(dirEntry);
                break;
            }
            ImGui::Text("/");
        }

        ImGui::Spring();
        ImGui::SetNextItemWidth(150.0f);
        if (UIUtils::SearchWidget(m_SearchString))
        {
            if (!m_SearchString.empty())
                m_DisplayList = ProjectLibrary::Get().Search(m_SearchString);
            UpdateDisplayList();
        }
        UI::ScopedStyle style(ImGuiStyleVar_LayoutAlign, 1);

        const float maxWidth = 150.0f * 1.1f;
        const float spacing = ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(" ").x;
        const float checkboxSize = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2;

        ImGui::SetNextItemWidth(150.0f);
        float thumbnailChange = m_ThumbnailSize;
        if (ImGui::SliderFloat("##iconsize", &thumbnailChange, MIN_ASSET_THUMBNAIL_SIZE, MAX_ASSET_THUMBNAIL_SIZE))
        {
            // m_Padding *= thumbnailChange / m_ThumbnailSize;
            m_ThumbnailSize = thumbnailChange;
            float cellSize = m_ThumbnailSize + m_Padding;
            float panelWidth = ImGui::GetContentRegionAvail().x;
            m_ColumnCount = (int)(panelWidth / cellSize);
            if (m_ColumnCount < 1)
                m_ColumnCount = 1;
        }

        ImGui::SetNextItemWidth(150.0f);
        static const char* sortingStr[3] = { "Sort By Name", "Sort By Size", "Sort By Date" };
        uint32_t currentMode = (uint32_t)m_FileSortingMode;
        if (ImGui::BeginCombo("##sort", sortingStr[currentMode]))
        {
            for (uint32_t i = 0; i < (uint32_t)FileSortingMode::SortCount; i++)
            {
                FileSortingMode mode = (FileSortingMode)i;
                if (ImGui::Selectable(sortingStr[i], i == currentMode))
                {
                    m_FileSortingMode = mode;
                    m_RequiresSort = true;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::EndHorizontal();
        ImGui::Spring();
        ImGui::EndVertical();
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
                for (const auto& entry : displayList)
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
            if (entry->Type == LibraryEntryType::Directory)
                SetCurrentDirectory(static_cast<DirectoryEntry*>(entry));
            else
                PlatformUtils::OpenExternally(entry->Filepath);
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
                for (auto& path : paths) // Maybe here I would need to remove the last char
                    ProjectLibrary::Get().CopyEntry(
                      path, EditorUtils::GetUniquePath(m_CurrentDirectoryEntry->Filepath / Path(path).filename()));
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
                m_SelectionEndIndex = displayList.size() - 1;
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
                    size_t lastIdx = displayList.size() - 1;
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
                    m_SelectionEndIndex = m_SelectionStartIndex =
                      std::min((int32_t)m_SelectionStartIndex + 1, (int32_t)displayList.size() - 1);
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
                    m_SelectionEndIndex = m_SelectionStartIndex =
                      std::max(0, (int32_t)(m_SelectionStartIndex - m_ColumnCount));
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
                    m_SelectionEndIndex = m_SelectionStartIndex =
                      std::min(m_SelectionStartIndex + m_ColumnCount, (uint32_t)displayList.size() - 1);
                    const Ref<LibraryEntry>& entry = displayList[m_SelectionStartIndex];
                    m_SelectionSet.insert(entry->ElementNameHash);
                    m_SetSelectedPathCallback(entry->Filepath);
                }
                else
                {
                    m_SelectionEndIndex =
                      std::min(m_SelectionEndIndex + m_ColumnCount, (uint32_t)displayList.size() - 1);

                    const Ref<LibraryEntry>& entry = displayList[m_SelectionEndIndex];
                    m_SetSelectedPathCallback(entry->Filepath);
                    shiftSelectionChanged = true;
                }
            }
            if (shiftSelectionChanged)
            {
                m_SelectionSet.clear();
                uint32_t startIdx = std::min(m_SelectionStartIndex, m_SelectionEndIndex);
                uint32_t endIdx = std::max(m_SelectionStartIndex, m_SelectionEndIndex);
                for (uint32_t i = startIdx; i <= endIdx; i++)
                    m_SelectionSet.insert(displayList[i]->ElementNameHash);
            }
        }
    }

    void AssetBrowserPanel::ClearSelection()
    {
        m_SelectionSet.clear();
        m_SelectionStartIndex = (uint32_t)-1;

        m_SetSelectedPathCallback({});
    }

    void AssetBrowserPanel::SortDisplayList(DisplayList& displayList) const
    {
        std::sort(
          displayList.begin(), displayList.end(), [this](const Ref<LibraryEntry>& l, const Ref<LibraryEntry>& r) {
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
        if (!m_SearchString.empty())
            m_DisplayList = ProjectLibrary::Get().Search(m_SearchString);
        else
        {
            m_DisplayList.clear();
            m_DisplayList.reserve(m_CurrentDirectoryEntry->Children.size());
            for (const Ref<LibraryEntry>& entry : m_CurrentDirectoryEntry->Children)
                m_DisplayList.push_back(entry);
        }
        m_RequiresSort = true;
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
        bool hovered = false;

        if (m_CurrentDirectoryEntry == nullptr)
        {
            ImGui::Columns(1);
            return;
        }

        if (ImGui::IsWindowFocused() && (m_RenamingPath.empty() || m_RenamingPath != m_CurrentDirectoryEntry->Filepath))
            HandleKeyboardNavigation();

        // Files
        const DisplayList& displayList = GetDisplayList();
        for (uint32_t entryIdx = 0; entryIdx < displayList.size(); entryIdx++)
        {
            const auto& entry = displayList[entryIdx];
            const auto& path = entry->Filepath;

            uint32_t upperBits = static_cast<uint32_t>(entry->ElementNameHash >> 32);
            uint32_t lowerBits = static_cast<uint32_t>(entry->ElementNameHash & 0xffffffff);
            ImGui::PushID(upperBits ^ lowerBits);

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
                auto iter = m_Icons.find(entry->ElementNameHash);
                if (iter != m_Icons.end())
                    tid = ImGui_ImplVulkan_AddTexture(iter->second);
                else
                    tid = m_FileIcon;
            }

            // Thumbnail
            ImGui::BeginGroup();
            ImGui::ImageButton(tid, { m_ThumbnailSize, m_ThumbnailSize }, { 0, 1 }, { 1, 0 }, 0);
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
                if (ImGui::InputText("##RenameFile", &m_RenamingText,
                                     ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue |
                                       ImGuiInputTextFlags_CodeSelectNoExt))
                    completeRename();
                ImGui::PopStyleVar();

                if ((Input::IsMouseButtonDown(Mouse::ButtonLeft) || Input::IsMouseButtonDown(Mouse::ButtonRight)) &&
                    !ImGui::IsItemClicked())
                    completeRename();

                if (Input::IsKeyPressed(Key::Escape))
                    completeRename();
            }

            ImGui::EndGroup();
            if (entryIdx == m_SelectionEndIndex)
                ImGui::ScrollToItem(ImGuiScrollFlags_KeepVisibleEdgeY);
            hovered |= ImGui::IsItemHovered();
            UI::SetTooltip(path.string().c_str());
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) // Allow dragging
            {
                UIUtils::SetAssetPayload(path);
                ImGui::ImageButton(tid, { m_ThumbnailSize, m_ThumbnailSize }, { 0, 1 }, { 1, 0 }, 0);
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
                        Path payloadPath = UIUtils::GetPathFromPayload(payload);
                        Path filename = payloadPath.filename();
                        ProjectLibrary::Get().MoveEntry(
                          payloadPath,
                          path / filename); // Perhaps I need to end here? Or rather I should change the display list
                        UpdateDisplayList();
                    }
                    ImGui::EndDragDropTarget();
                }
            }

            if (!selected)
                ImGui::PopStyleColor();
            ImGui::PopStyleColor();

            // Only enter the directory if we click on the image/text but not if we double click in the InputText for
            // renaming
            if (ImGui::IsItemHovered() && m_RenamingText.empty() &&
                ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) // Enter directory
            {
                if (entry->Type == LibraryEntryType::Directory)
                    SetCurrentDirectory(static_cast<DirectoryEntry*>(entry.get()));
                else // Open the file
                {
                    // Note: Could directly open the file in the editor,
                    // Now I need to associate the file type with Crowny and also have some "only one app instance" rule
                    // and IPC
                    PlatformUtils::OpenExternally(path);
                }
            }

            if (Input::IsMouseButtonDown(Mouse::ButtonLeft) || Input::IsMouseButtonDown(Mouse::ButtonRight))
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
                        m_SetSelectedPathCallback(path);
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
            ImGui::NextColumn();
        }

        if (Input::IsMouseButtonDown(Mouse::ButtonLeft) && !hovered && !ImGui::IsItemHovered() &&
            ImGui::IsWindowHovered())
            ClearSelection();

        ImGui::Columns(1);
    }

    void AssetBrowserPanel::DrawTreeView()
    {
        ImGui::Begin("Tree view");

        std::function<void(const Ref<LibraryEntry>&, int32_t)> display = [&](const Ref<LibraryEntry>& cur,
                                                                             int32_t dirEntryIdx = -1) {
            if (cur->Type == LibraryEntryType::Directory)
            {
                DirectoryEntry* dirEntry = static_cast<DirectoryEntry*>(cur.get());

                // Need to check all children since we are only looking for directories and not files.
                bool hasChildren = false;
                for (const auto& child : dirEntry->Children)
                    if (child->Type == LibraryEntryType::Directory)
                        hasChildren = true;
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | (hasChildren ? 0 : ImGuiTreeNodeFlags_Leaf);

                if (m_CurrentDirectoryEntry->ElementNameHash == cur->ElementNameHash &&
                    m_CurrentDirectoryEntry->Filepath == cur->Filepath)
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
                    if (const ImGuiPayload* payload = UIUtils::AcceptAssetPayload())
                    {
                        Path payloadPath = UIUtils::GetPathFromPayload(payload);
                        ProjectLibrary::Get().MoveEntry(payloadPath, cur->Filepath / payloadPath.filename());
                        UpdateDisplayList();
                    }
                    ImGui::EndDragDropTarget();
                }
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) // Allow dragging
                {
                    UIUtils::SetAssetPayload(cur->Filepath);
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
            m_SearchString
              .clear(); // Clear the search so we can go back to the original directory and finish creation there
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
        String filename = GetDefaultFileNameFromType(itemType);
        Path newEntryPath = EditorUtils::GetUniquePath(m_CurrentDirectoryEntry->Filepath / filename);
        switch (itemType)
        {
        case AssetBrowserItem::Folder:
            ProjectLibrary::Get().CreateFolderEntry(newEntryPath);
            break;
        case AssetBrowserItem::CScript: {
            String text = GetDefaultContents(itemType);
            String className = newEntryPath.filename().replace_extension("").string();
            className = StringUtils::Replace(className, " ", "_");
            String script =
              StringUtils::Replace(text, "#NAMESPACE#", Editor::Get().GetProjectPath().filename().string());
            script = StringUtils::Replace(script, "#CLASSNAME#",
                                          className); // This has to be done after rename, since the file is saved first
                                                      // as NewScript and then as the user name.
            FileSystem::WriteTextFile(newEntryPath, script);
            break;
        }
        case AssetBrowserItem::PhysicsMaterial: {
            ProjectLibrary::Get().CreateEntry(CreateRef<PhysicsMaterial2D>(), newEntryPath);
            break;
        }
        default: {
            FileSystem::WriteTextFile(newEntryPath, GetDefaultContents(itemType));
            break;
        }
        }
        ProjectLibrary::Get().Refresh(newEntryPath);
        m_RenamingPath = ProjectLibrary::Get().FindEntry(newEntryPath)->Filepath;
        m_RenamingText = newEntryPath.filename().string();
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
        m_SearchString.clear(); // TODO: Don't do this here
    }

    String AssetBrowserPanel::GetDefaultContents(AssetBrowserItem itemType)
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
