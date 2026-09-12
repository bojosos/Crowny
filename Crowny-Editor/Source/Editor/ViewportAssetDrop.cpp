#include "cwepch.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Scene/EntityInstantiation.h"
#include "Crowny/Scene/Prefab.h"
#include "Editor/MaterialEditing.h"
#include "Editor/UndoRedo.h"
#include "Editor/ViewportAssetDrop.h"
#include "Panels/ViewportHudText.h"

namespace Crowny
{
    namespace
    {
        using DropAction = Entity (*)(const AssetHandle<Asset>&, const FileEntry&, const ViewportDropContext&);
        struct Handler
        {
            AssetType Type;
            ViewportDropFileKind SourceKind;
            const char* Label;
            DropAction Apply;
        };

        Entity DropMesh(const AssetHandle<Asset>& asset, const FileEntry& file, const ViewportDropContext& context)
        {
            if (!context.WorldPosition)
                return {};
            Entity entity = context.TargetScene->CreateEntity(file.Filepath.filename().string());
            entity.AddComponent<MeshRendererComponent>().MeshHandle = static_asset_cast<Mesh>(asset);
            return entity;
        }

        Entity DropAudio(const AssetHandle<Asset>& asset, const FileEntry& file, const ViewportDropContext& context)
        {
            Entity entity = context.TargetScene->CreateEntity(file.Filepath.filename().string());
            entity.AddComponent<AudioSourceComponent>().SetClip(static_asset_cast<AudioClip>(asset));
            return entity;
        }

        Entity DropPrefab(const AssetHandle<Asset>& asset, const FileEntry&, const ViewportDropContext& context)
        {
            EntityInstantiateOptions options;
            options.Parent = context.TargetScene->GetRootEntity();
            return EntityInstantiator::InstantiatePrefab(*context.TargetScene, static_asset_cast<Prefab>(asset), options);
        }

        Entity DropMaterial(const AssetHandle<Asset>& asset, const FileEntry&, const ViewportDropContext& context)
        {
            if (context.TargetEntity && context.TargetEntity.GetScene() == context.TargetScene.get())
                UndoRedo::Get().RegisterAction(AssignViewportMaterial(context.TargetEntity, static_asset_cast<Material>(asset)));
            return {};
        }

        // Add an asset type here: its source classification, hover label and action stay together.
        const Handler Handlers[] = {
            { AssetType::Scene, ViewportDropFileKind::Scene, "Drop to open scene", nullptr },
            { AssetType::Mesh, ViewportDropFileKind::Mesh, "Drop to create mesh entity", DropMesh },
            { AssetType::Material, ViewportDropFileKind::Material, "Drop to apply material", DropMaterial },
            { AssetType::AudioClip, ViewportDropFileKind::AudioClip, "Drop to create audio source", DropAudio },
            { AssetType::Prefab, ViewportDropFileKind::Prefab, "Drop to instantiate prefab", DropPrefab },
        };

        const Handler* FindHandler(const Path& path, const FileEntry* file)
        {
            const auto sourceKind = ClassifyViewportDropFile(path);
            for (const Handler& handler : Handlers)
            {
                if ((file && file->Metadata) ? file->Metadata->Type == handler.Type : sourceKind == handler.SourceKind)
                    return &handler;
            }
            return nullptr;
        }
    } // namespace

    const char* ViewportAssetDrop::Describe(const Path& path) const
    {
        const Ref<FileEntry> file = m_Library.Find(path);
        const Handler* handler = FindHandler(path, file.get());
        return handler ? handler->Label : nullptr;
    }

    bool ViewportAssetDrop::Submit(const Path& path, const ViewportDropContext& context, double now)
    {
        if (!context.TargetScene || !Describe(path))
            return false;
        const Ref<FileEntry> file = m_Library.Find(path);
        if (file && file->Metadata)
        {
            Apply(*file, context);
            return true;
        }
        const Path imported = m_Library.Import(path);
        if (imported.empty())
            return false;
        m_Pending.push_back({ imported.lexically_normal(), context, now });
        return true;
    }

    void ViewportAssetDrop::Update(const Ref<Scene>& activeScene, bool editing, double now)
    {
        // Remove a request before invoking actions: opening a scene can re-enter editor code.
        for (size_t index = 0; index < m_Pending.size();)
        {
            const Pending& pending = m_Pending[index];
            if (!editing || pending.Context.TargetScene != activeScene)
            {
                m_Pending.erase(m_Pending.begin() + static_cast<std::ptrdiff_t>(index));
                continue;
            }
            const Ref<FileEntry> file = m_Library.Find(pending.AssetPath);
            if (file && file->Metadata)
            {
                const ViewportDropContext context = pending.Context;
                m_Pending.erase(m_Pending.begin() + static_cast<std::ptrdiff_t>(index));
                Apply(*file, context);
                // A scene-open action invalidates the remaining captured targets.
                if (file->Metadata->Type == AssetType::Scene)
                {
                    m_Pending.clear();
                    return;
                }
                continue;
            }
            if (!m_Library.IsImporting() && now - pending.QueuedAt > (file ? 2.0 : 60.0))
            {
                CW_ENGINE_WARN("Dropped file '{}' was not imported; nothing was added to the scene.", pending.AssetPath);
                m_Pending.erase(m_Pending.begin() + static_cast<std::ptrdiff_t>(index));
                continue;
            }
            ++index;
        }
    }

    void ViewportAssetDrop::Apply(const FileEntry& file, const ViewportDropContext& context)
    {
        const Handler* handler = FindHandler(file.Filepath, &file);
        if (!handler || !file.Metadata)
            return;
        if (handler->Type == AssetType::Scene)
        {
            m_Pending.clear();
            if (m_Actions.OpenScene)
                m_Actions.OpenScene(file.Metadata->Uuid);
            return;
        }
        const AssetHandle<Asset> asset = m_Library.Load(file);
        if (!asset.IsLoaded() || asset->GetAssetType() != handler->Type)
        {
            CW_ENGINE_WARN("Dropped asset '{}' could not be loaded.", file.Filepath);
            return;
        }
        Entity created = handler->Apply(asset, file, context);
        if (!created)
            return;
        if (context.WorldPosition)
            created.GetTransform().SetWorldPosition(*context.WorldPosition, created.GetParent());
        UndoRedo::Get().RegisterAction(CreateRef<EntityCreatedAction>(created, context.TargetScene));
        if (m_Actions.SelectEntity)
            m_Actions.SelectEntity(created);
    }
} // namespace Crowny
