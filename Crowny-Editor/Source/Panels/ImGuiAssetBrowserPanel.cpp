#include "cwpch.h"

#include "Panels/ImGuiAssetBrowserPanel.h"

#include "Editor/EditorAssets.h"
#include "Crowny/Input/Input.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Common/PlatformUtils.h"

#include <backends/imgui_impl_vulkan.h>
#include <imgui_internal.h>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <filesystem>

namespace Crowny
{
    
    static const std::filesystem::path s_AssetPath = "Resources";
    extern void LoadTexture(const std::string& filepath, Ref<Texture>& texture);
    
    static std::string GetDefaultFileNameFromType(AssetBrowserItem type)
    {
        switch(type)
        {
            case AssetBrowserItem::CScript:          return "NewScript.cs";
            case AssetBrowserItem::Folder:           return "New Folder";
            case AssetBrowserItem::Material:         return "New Material.mat";
            case AssetBrowserItem::Prefab:           return "New Prefab.prefab";
            case AssetBrowserItem::Shader:           return "New Shader.shader";
            case AssetBrowserItem::ComputeShader:    return "New ComputeShader.cshader";
            case AssetBrowserItem::PhysicsMaterial:  return "New PhysicsMaterial.pmat";
            case AssetBrowserItem::RenderTexture:    return "New RenderTexture.rt";
            case AssetBrowserItem::Scene:            return "New Scene.cwscene";
            default:                                 return "New File";
        }
    }
    
	ImGuiAssetBrowserPanel::ImGuiAssetBrowserPanel(const std::string& name) : ImGuiPanel(name), m_CurrentDirectory(s_AssetPath)
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
                LoadTexture(path, result);
                m_Textures[filename] = result;
            }
        }
	}

	void ImGuiAssetBrowserPanel::Render()
	{
		UpdateState();
		ImGui::Begin("Asset browser", &m_Shown, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
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
        
        ImGui::Separator();
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_HorizontalScrollbar;
        ImGui::BeginChild("child", ImVec2(0, 0), false, window_flags);
        if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_NoOpenOverExistingPopup | ImGuiPopupFlags_MouseButtonRight))
        {
            ShowContextMenuContents();
            ImGui::EndPopup();
        }
        static float padding = 12.0f;
        static float thumbnailSize = 48.0f;
        float cellSize = thumbnailSize + padding;
        float panelWidth = ImGui::GetContentRegionAvail().x;
        int columnCount = (int)(panelWidth / cellSize);
        if (columnCount < 1)
            columnCount = 1;
        ImGui::Columns(columnCount, 0, false);
        
        if (!m_RenamingPath.empty())
        {
            ImTextureID tid;
            switch(m_RenamingType)
            {
                case AssetBrowserItem::Folder: tid = m_FolderIcon;
                default: tid = m_FileIcon;
            }
            ImGui::ImageButton(tid, { thumbnailSize, thumbnailSize }, { 0, 1 }, { 1, 0 }/*, -1, ImGui::GetStyleColorVec4(ImGuiCol_Header)*/);
            m_Filename = GetDefaultFileNameFromType(m_RenamingType);
            ImGui::SetKeyboardFocusHere();
            auto completeRename = [&]()
            {
                if (m_RenamingType == AssetBrowserItem::Folder)
                {
                    if (!std::filesystem::create_directory(m_CurrentDirectory / m_Filename))
                        CW_ENGINE_ERROR("Error creating directory {0}", (m_CurrentDirectory / m_Filename).string());
                }
                else if (m_RenamingType == AssetBrowserItem::CScript)
                {
                    std::filesystem::copy_file(EditorAssets::DefaultScriptPath.c_str(), (m_CurrentDirectory / m_Filename).c_str());
                }
                else
                    CW_ENGINE_INFO("How make file");
                m_RenamingPath.clear();
            };
            ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 5));
            if (ImGui::InputText("##renaming", &m_Filename, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CodeSelectNoExt))
                completeRename();
            ImGui::PopStyleVar();
            
            if ((ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)) && !ImGui::IsItemClicked())
                completeRename();

            ImGui::NextColumn();
        }
        
        std::map<long, std::filesystem::directory_entry> sortedFiles; // assumes time_t is long
        if (m_FileSorting == FileSorting::SortByName)
        {
            std::set<std::filesystem::directory_entry> sortedDirs;
            for (auto& dir : std::filesystem::directory_iterator(m_CurrentDirectory))
            {
                sortedDirs.insert(dir);
            }
            
            long id = 0;
            for (auto& dir : sortedDirs)
            {
                sortedFiles[id++] = dir;
            }
        }
        else
        {
            std::set<std::filesystem::directory_entry> folders;
            for (auto& dir : std::filesystem::directory_iterator(m_CurrentDirectory))
            {
                if (m_FileSorting == FileSorting::SortBySize)
                {
                    if (dir.is_regular_file())
                    {
                        long id = dir.file_size();
                        sortedFiles[id] = dir;
                    }
                    else if (dir.is_directory())
                        folders.insert(dir);
                }
                if (m_FileSorting == FileSorting::SortByDate)
                {
                    if (dir.is_regular_file())
                    {
                        long id = dir.file_size();
                        sortedFiles[id] = dir;
                    }
                    else if (dir.is_directory())
                        folders.insert(dir);
                }
            }
            long id = LONG_LONG_MAX - folders.size() - 10;
            for (auto& folder : folders)
            {
                sortedFiles[id++] = folder;
            }
        }
        
        for (auto& entry : sortedFiles)
        {
            std::filesystem::directory_entry dir = entry.second;
            const auto& path = dir.path();
            auto relativePath = std::filesystem::relative(path, s_AssetPath);
            std::string filename = relativePath.filename().string();
            
            auto iterFind = m_SelectedFiles.find(filename);
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
            
            ImGui::BeginGroup();
            ImGui::ImageButton(tid, { thumbnailSize, thumbnailSize }, { 0, 1 }, { 1, 0 }/*, -1, ImGui::GetStyleColorVec4(ImGuiCol_Header)*/);
            ImGui::TextWrapped("%s", filename.c_str());
            ImGui::EndGroup();
            
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                const char* itemPath = relativePath.c_str();
                ImGui::SetDragDropPayload("ASSET_ITEM", itemPath, strlen(itemPath) * sizeof(char));
                ImGui::ImageButton(tid, { thumbnailSize, thumbnailSize }, { 0, 1 }, { 1, 0 }/*, -1, ImGui::GetStyleColorVec4(ImGuiCol_Header)*/);
                ImGui::TextWrapped("%s", filename.c_str());
                
                ImGui::EndDragDropSource();
            }
            
            if (dir.is_directory())
            {
                if (ImGui::BeginDragDropTarget())
                {
                    const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_ITEM");
                    if (payload)
                    {
                        const char* path = (const char*)payload->Data;
                        CW_ENGINE_INFO(path);
                        if (std::rename((s_AssetPath / path).c_str(), (s_AssetPath / relativePath / path).c_str()) < 0)
                            CW_ENGINE_ERROR("Error moving file: {0}, {1} -> {2}", strerror(errno), path, (relativePath / filename).string());
                    }
                    ImGui::EndDragDropTarget();
                }
            }
            
            if (!selected)
                ImGui::PopStyleColor();
            ImGui::PopStyleColor();
            
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                if (dir.is_directory())
                {
                    m_PreviousDirectory.clear();
                    m_CurrentDirectory /= path.filename();
                    m_SelectedFiles.clear();
                }
            }
            
            if (ImGui::IsItemHovered() && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
            {
                if (Input::IsKeyPressed(Key::LeftControl))
                {
                    if (selected)
                        m_SelectedFiles.erase(iterFind);
                    else
                        m_SelectedFiles.insert(filename);
                }
                else
                {
                    m_SelectedFiles.clear();
                    m_SelectedFiles.insert(filename);
                }
            }
            if (ImGui::BeginPopupContextItem(filename.c_str()))
            {
                ShowContextMenuContents(relativePath.string());
                ImGui::EndPopup();
            }
            ImGui::NextColumn();
        }
        
        
        
        ImGui::Columns(1);
        ImGui::EndChild();
        ImGui::SliderFloat("Icon size", &thumbnailSize, 16, 512);
        ImGui::SameLine();
        ImGui::SliderFloat("Padding", &padding, 0, 32);
        
		ImGui::End();
	}

    void ImGuiAssetBrowserPanel::ShowContextMenuContents(const std::string& filepath)
    {
        if (ImGui::BeginMenu("Create"))
        {
            if (ImGui::MenuItem("Folder"))
            {
                m_RenamingType = AssetBrowserItem::Folder;
                m_RenamingPath = m_CurrentDirectory;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("C# script"))
            {
                m_RenamingType = AssetBrowserItem::CScript;
                m_RenamingPath = m_CurrentDirectory;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Scene"))
            {
                
            }
            if (ImGui::MenuItem("Prefab"))
            {
                
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Material"))
            {
                
            }
            if (ImGui::MenuItem("Shader"))
            {
                
            }
            if (ImGui::MenuItem("Compute Shader"))
            {
                
            }
            if (ImGui::MenuItem("Render Texture"))
            {
                
            }
            if (ImGui::MenuItem("Physics Material"))
            {
            }
                
            ImGui::EndMenu();
        }
        
        if (ImGui::MenuItem("Show In Explorer"))
        {
            if (filepath.empty())
                PlatformUtils::ShowInExplorer(m_CurrentDirectory);
            else
                PlatformUtils::ShowInExplorer(s_AssetPath / filepath);
        }
        
        if (filepath.empty()) ImGui::PushDisabled();
        if (ImGui::MenuItem("Open"))
        {
            PlatformUtils::OpenExternally(filepath);
        }
        
        if (ImGui::MenuItem("Delete"))
        {
            if (!std::filesystem::remove((s_AssetPath / filepath).c_str()))
                CW_ENGINE_ERROR("Error deleting file.");
        }
        
        if (ImGui::MenuItem("Rename"))
        {
            m_RenamingPath = filepath;
            //m_RenamingType = 
        }
        
        if (filepath.empty()) ImGui::PopDisabled();
        
        if (ImGui::MenuItem("Copy Path"))
        {
            if (!filepath.empty())
                PlatformUtils::CopyToClipboard(std::filesystem::absolute(s_AssetPath / filepath));
            else
                PlatformUtils::CopyToClipboard(std::filesystem::absolute(m_CurrentDirectory));
        }
        
        ImGui::Separator();
        if (ImGui::MenuItem("Refresh", "Ctrl+R"))
        {
            
        }
    }

    void ImGuiAssetBrowserPanel::Show()
	{
		m_Shown = true;
	}

	void ImGuiAssetBrowserPanel::Hide()
	{
		m_Shown = false;
	}
    
}