#pragma once

#include "Panels/ImGuiPanel.h"

#include "Editor/EditorDefaults.h"
#include "Editor/ProjectLibrary.h"

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

    typedef void* ImTextureID;

    class AssetBrowserPanel : public ImGuiPanel
    {
    public:
        AssetBrowserPanel(const String& name, std::function<void(const Path&)> selectedPathCallback);
        ~AssetBrowserPanel() = default;

        virtual void Render() override;

        void Initialize();

        const Path& GetCurrentEntryPath() const { return m_CurrentDirectoryEntry->Filepath; }

    private:
        void ShowContextMenuContents(LibraryEntry* entry = nullptr, bool isTreeView = false);
        void DrawHeader();
        void DrawFiles();
        void DrawTreeView();
        void CreateNew(AssetBrowserItem itemType);
        String GetDefaultContents(AssetBrowserItem itemType);
        void HandleKeyboardNavigation();
        void RecalculateDirectoryEntries();

    private:
        Vector<DirectoryEntry*> m_DirectoryPathEntries;
        Vector<Ref<LibraryEntry>> m_DisplayList;
        String m_SearchString;
        uint32_t m_ColumnCount = 5;
        ImTextureID m_FolderIcon;
        ImTextureID m_FileIcon;

        UnorderedMap<size_t, Ref<Texture>> m_Icons; // For showing the textures in the asset browser.

        String m_CsDefaultText;

        Vector<Path> m_OrderedSelection;
        UnorderedSet<size_t> m_SelectionSet;
        uint32_t m_SelectionStartIndex = 0;

        DirectoryEntry* m_CurrentDirectoryEntry = nullptr;

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