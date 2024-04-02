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
        using DisplayList = Vector<Ref<LibraryEntry>>;

        void SetCurrentDirectory(DirectoryEntry* entry);

        void HandleOpen(LibraryEntry* entry);
        void ShowContextMenuContents(LibraryEntry* entry = nullptr, bool isTreeView = false);
        void DrawHeader();
        void DrawFiles();
        void DrawTreeView();
        void CreateNew(AssetBrowserItem itemType);
        String GetDefaultContents(AssetBrowserItem itemType);
        void HandleKeyboardNavigation();
        void RecalculateDirectoryEntries();
        void ClearSelection();
        const DisplayList& GetDisplayList();
        void UpdateDisplayList();

        void GoForward();
        void GoBackward();
        void SortDisplayList(DisplayList& displayList) const;

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
        uint32_t m_SelectionStartIndex = (uint32_t)-1;
        uint32_t m_SelectionEndIndex = 0;
        size_t m_LastCurrentDirectory = 0;

        DirectoryEntry* m_CurrentDirectoryEntry = nullptr;

        Stack<DirectoryEntry*> m_BackwardHistory;
        Stack<DirectoryEntry*> m_ForwardHistory;
        bool m_RequiresSort = true;

        FileSortingMode m_FileSortingMode = FileSortingMode::SortByName;

        float m_Padding = 12.0f;
        float m_ThumbnailSize = DEFAULT_ASSET_THUMBNAIL_SIZE;

        Path m_RenamingPath;
        String m_RenamingText;

        std::function<void(const Path&)> m_SetSelectedPathCallback;
    };

} // namespace Crowny