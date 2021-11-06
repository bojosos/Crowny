#pragma once

#include "ImGuiPanel.h"

#include "Editor/EditorDefaults.h"
#include "Editor/ProjectLibrary.h"

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
        SortCount = 4
    };

    class ImGuiAssetBrowserPanel : public ImGuiPanel
    {
    public:
        ImGuiAssetBrowserPanel(const String& name, std::function<void(const Path&)> selectedPathCallback);
        ~ImGuiAssetBrowserPanel() = default;

        virtual void Render() override;

        virtual void Show() override;
        virtual void Hide() override;

        void Initialize();

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
        Set<Path> m_SelectedFiles;
        DirectoryEntry* m_CurrentDirectoryEntry;
        String m_CsDefaultText;

        Stack<DirectoryEntry*> m_BackwardHistory;
        Stack<DirectoryEntry*> m_ForwardHistory;

        FileSortingMode m_FileSortingMode = FileSortingMode::SortBySize;
        float m_Padding = 12.0f;
        float m_ThumbnailSize = DEFAULT_ASSET_THUMBNAIL_SIZE;
        String m_Filename;
        AssetBrowserItem m_RenamingType;
        std::filesystem::path m_RenamingPath;

        std::function<void(const Path&)> m_SetSelectedPathCallback;
    };

} // namespace Crowny