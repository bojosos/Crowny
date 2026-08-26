#pragma once

#include "Panels/AssetBrowserOperations.h"
#include "Panels/EditorPanelRegistration.h"
#include "Panels/ImGuiPanel.h"

#include "Editor/AssetPreviewService.h"
#include "Editor/EditorDefaults.h"
#include "Editor/ProjectLibrary.h"

#include <optional>

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
        PhysicsMaterial2D,
        PhysicsMaterial3D,
        NodeGraph
    };

    enum class FileSortingMode
    {
        SortByName = 0,
        SortBySize = 1,
        SortByDate = 2,
        SortCount = 3
    };

    enum class AssetBrowserFilter
    {
        All,
        Scenes,
        Images,
        Materials,
        Models,
        Audio,
        Code,
        Count
    };

    enum class AssetBrowserView
    {
        Grid,
        List
    };

    class AssetBrowserPanel : public ImGuiPanel
    {
    public:
        inline static constexpr EditorPanelRegistration<AssetBrowserPanel> Registration{ "Asset Browser", "View/Asset Browser" };

        AssetBrowserPanel(const String& name, std::function<void(const Path&)> selectedPathCallback);
        ~AssetBrowserPanel() = default;

        virtual void Render() override;

        void Initialize();
        void Unload();

        const Path& GetCurrentEntryPath() const { return m_CurrentDirectoryEntry->Filepath; }

    private:
        using DisplayList = Vector<Ref<LibraryEntry>>;

        void SetCurrentDirectory(DirectoryEntry* entry);
        void HandleOpen(LibraryEntry* entry);
        void ShowContextMenuContents(LibraryEntry* entry = nullptr, bool isTreeView = false);
        void DrawHeader();
        void DrawFiles();
        void DrawStatusBar() const;
        void DrawTreeView();
        void CreateNew(AssetBrowserItem itemType);
        String GetDefaultContents(AssetBrowserItem itemType) const;
        void HandleKeyboardNavigation();
        void RecalculateDirectoryEntries();
        void ClearSelection();
        void ReconcileSelection(const std::optional<Path>& preferredStartPath = {}, const std::optional<Path>& preferredEndPath = {});
        void ApplyDeferredOperations();
        const DisplayList& GetDisplayList();
        void UpdateDisplayList(const std::optional<Path>& preferredStartPath = {}, const std::optional<Path>& preferredEndPath = {});

        void GoForward();
        void GoBackward();
        void SortDisplayList(DisplayList& displayList) const;
        Vector<AssetType> GetActiveAssetTypes() const;

    private:
        Vector<DirectoryEntry*> m_DirectoryPathEntries;
        Vector<Ref<LibraryEntry>> m_DisplayList;
        String m_SearchString;
        uint32_t m_ColumnCount = 5;
        ImTextureID m_FolderIcon;
        ImTextureID m_FileIcon;

        AssetPreviewService m_PreviewService;

        String m_CsDefaultText;

        Vector<Path> m_OrderedSelection;
        Vector<const Path*> m_DisplayEntryPaths;
        Vector<const Path*> m_SortedDisplayEntryPaths;
        UnorderedSet<Path, HashPath> m_SelectionSet;
        uint32_t m_SelectionStartIndex = (uint32_t)-1;
        uint32_t m_SelectionEndIndex = 0;
        size_t m_LastCurrentDirectory = 0;

        DirectoryEntry* m_CurrentDirectoryEntry = nullptr;

        Stack<DirectoryEntry*> m_BackwardHistory;
        Stack<DirectoryEntry*> m_ForwardHistory;
        bool m_RequiresSort = true;

        FileSortingMode m_FileSortingMode = FileSortingMode::SortByName;
        AssetBrowserFilter m_AssetFilter = AssetBrowserFilter::All;
        AssetBrowserView m_View = AssetBrowserView::Grid;

        float m_Padding = 12.0f;
        float m_ThumbnailSize = DEFAULT_ASSET_THUMBNAIL_SIZE;

        Path m_RenamingPath;
        String m_RenamingText;
        AssetBrowserOperationQueue m_DeferredOperations;

        std::function<void(const Path&)> m_SetSelectedPathCallback;
    };

} // namespace Crowny
