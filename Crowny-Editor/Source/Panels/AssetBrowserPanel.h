#pragma once

#include "Panels/ImGuiPanel.h"

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
        SortCount = 3
    };

    class AssetBrowserPanel : public ImGuiPanel
    {
    public:
        AssetBrowserPanel(const String& name, std::function<void(const Path&)> selectedPathCallback);
        ~AssetBrowserPanel() = default;

        virtual void Render() override;

        virtual void Show() override;
        virtual void Hide() override;

        void Initialize();

    private:
        void ShowContextMenuContents(LibraryEntry* entry = nullptr);
        void DrawHeader();
        void DrawFiles();
        void CreateNew(AssetBrowserItem itemType);
        String GetDefaultContents(AssetBrowserItem itemType);

    private:
        ImTextureID m_FolderIcon;
        ImTextureID m_FileIcon;

        UnorderedMap<size_t, Ref<Texture>> m_Textures; // For showing the textures in the asset browser.
        String m_CsDefaultText;

        Vector<Path> m_OrderedSelection;
        UnorderedSet<size_t> m_SelectionSet;
        uint32_t m_SelectionStartIndex;

        DirectoryEntry* m_CurrentDirectoryEntry;

        Stack<DirectoryEntry*> m_BackwardHistory;
        Stack<DirectoryEntry*> m_ForwardHistory;

        FileSortingMode m_FileSortingMode = FileSortingMode::SortBySize;

        float m_Padding = 12.0f;
        float m_ThumbnailSize = DEFAULT_ASSET_THUMBNAIL_SIZE;

        Path m_RenamingPath;
        String m_RenamingText;

        std::function<void(const Path&)> m_SetSelectedPathCallback;
    };

} // namespace Crowny