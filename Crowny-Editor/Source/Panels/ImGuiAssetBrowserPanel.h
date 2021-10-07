#pragma once

#include "Editor/EditorDefaults.h"
#include "ImGuiPanel.h"

#include <filesystem>
#include <imgui.h>

namespace Crowny
{

    enum class AssetBrowserItem
    {
        Folder,
        CScript,
        Scene,
        Prefab,
        Material,
        Texture,
        RenderTexture,
        Shader,
        ComputeShader,
        PhysicsMaterial
    };

    enum class FileSortingMode
    {
        SortByName = 0,
        SortBySize = 1,
        SortByDate = 2,
        SortByType = 3, // not supported, slightly harder to implement
        SortCount = 4
    };

    class ImGuiAssetBrowserPanel : public ImGuiPanel
    {
    public:
        ImGuiAssetBrowserPanel(const String& name);
        ~ImGuiAssetBrowserPanel() = default;

        virtual void Render() override;

        virtual void Show() override;
        virtual void Hide() override;

    private:
        void ShowContextMenuContents(const Path& filepath = "");
        void DrawHeader();
        void DrawFiles();
        void CreateNew(AssetBrowserItem itemType);
        String GetDefaultContents(AssetBrowserItem itemType);

    private:
        ImTextureID m_FolderIcon;
        ImTextureID m_FileIcon;

        UnorderedMap<String, Ref<Texture>> m_Textures; // For showing the textures in the asset browser.
        Set<String> m_SelectedFiles;
        std::filesystem::path m_PreviousDirectory;
        std::filesystem::path m_CurrentDirectory;

        String m_CsDefaultText;

        FileSortingMode m_FileSortingMode = FileSortingMode::SortBySize;
        float m_Padding = 12.0f;
        float m_ThumbnailSize = DEFAULT_ASSET_THUMBNAIL_SIZE;
        String m_Filename;
        AssetBrowserItem m_RenamingType;
        std::filesystem::path m_RenamingPath;
    };

} // namespace Crowny