#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Ecs/Entity.h"
#include "Editor/AssetLibraryTypes.h"

#include <functional>
#include <optional>

namespace Crowny
{
    struct ViewportDropContext
    {
        Ref<Scene> TargetScene;
        Entity TargetEntity;
        std::optional<glm::vec3> WorldPosition;
    };

    // Both OS paths and asset-browser paths cross this interface. Import completion,
    // scene lifetime, supported types and their actions are independent of ImGui.
    class ViewportAssetDrop
    {
    public:
        struct Library
        {
            std::function<Ref<FileEntry>(const Path&)> Find;
            std::function<Path(const Path&)> Import;
            std::function<AssetHandle<Asset>(const FileEntry&)> Load;
            std::function<bool()> IsImporting;
        };

        struct Actions
        {
            std::function<void(const UUID&)> OpenScene;
            std::function<void(Entity)> SelectEntity;
        };

        explicit ViewportAssetDrop(Library library) : m_Library(std::move(library)) {}
        void SetActions(Actions actions) { m_Actions = std::move(actions); }

        // Null means unsupported. The label and acceptance come from the same handler.
        const char* Describe(const Path& path) const;
        bool Submit(const Path& path, const ViewportDropContext& context, double now);
        void Update(const Ref<Scene>& activeScene, bool editing, double now);
        size_t GetPendingCount() const { return m_Pending.size(); }

    private:
        struct Pending
        {
            Path AssetPath;
            ViewportDropContext Context;
            double QueuedAt;
        };

        void Apply(const FileEntry& file, const ViewportDropContext& context);
        Library m_Library;
        Actions m_Actions;
        Vector<Pending> m_Pending;
    };

    ViewportAssetDrop CreateProjectViewportAssetDrop();
} // namespace Crowny
