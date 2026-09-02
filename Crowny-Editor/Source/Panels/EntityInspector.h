#pragma once

#include "Crowny/Ecs/Entity.h"
#include "Editor/ComponentUndoSnapshot.h"
#include "Editor/SelectionComponentOperations.h"
#include "Editor/UndoRedo.h"
#include "Panels/ComponentMenuModel.h"

#include <imgui.h>

#include <entt/entt.hpp>

namespace Crowny
{
    // Renders the properties of one component on a single entity.
    template <class Component> void ComponentEditorWidget(Entity entity) {}

    // Renders the properties of one component across the whole selection. The default
    // falls back to the single-entity widget and refuses multi-selection editing.
    template <class Component> void ComponentSelectionEditorWidget(Entity primary, const Vector<Entity>& entities)
    {
        if (entities.size() == 1u)
        {
            ComponentEditorWidget<Component>(primary);
            return;
        }
        ImGui::Columns(1);
        ImGui::TextDisabled("This component cannot be edited across multiple entities yet.");
        ImGui::Columns(2);
    }

    template <class Component> SelectionComponentChange ComponentSelectionAddAction(std::span<const Entity> entities)
    {
        return AddComponentToSelection<Component>(entities);
    }

    template <class Component> Ref<UndoAction> ComponentRemoveAction(Entity entity)
    {
        auto comp = entity.GetComponent<Component>();
        entity.RemoveComponent<Component>();
        return CreateRef<Crowny::RemoveComponentAction<Component>>(entity, comp);
    }

    using ComponentWidgetCallback = std::function<void(Entity, const Vector<Entity>&)>;

    // Customization point for how a component type integrates with the entity inspector.
    // Specialize it (see ScriptComponentInspector.h) for components that record undo
    // differently or draw their own headers; the generic inspector never names them.
    template <class Component> struct ComponentInspectorTraits
    {
        // Whether the type appears in the "Add component" browser.
        static constexpr bool ListedInAddMenu = true;
        // Whether the widget draws its own collapsing header(s) instead of the standard one.
        static constexpr bool DrawsOwnHeader = false;

        static Ref<RetainedUndoActionFactory> CreateUndoFactory() { return CreateRef<ComponentUndoSnapshot<Component>>(); }

        // Runs `widget` inside an undo scope so edits made this frame become one retained action.
        static void RenderWithUndo(const Ref<RetainedUndoActionFactory>& factory, const ComponentWidgetCallback& widget, Entity primary,
                                   const Vector<Entity>& entities)
        {
            Ref<ComponentUndoSnapshot<Component>> snapshots = StaticRefCast<ComponentUndoSnapshot<Component>>(factory);
            UndoRedo& undoRedo = UndoRedo::Get();
            if (undoRedo.BeginComponentScope(snapshots))
                snapshots->Capture(entities);
            widget(primary, entities);
            snapshots->CompleteFrame();
            undoRedo.EndComponentScope();
        }
    };

    // The body of the inspector for an entity selection: the entity header, the prefab
    // instance header, the component list and the "Add component" popup.
    class EntityInspector
    {
    public:
        using ComponentTypeID = entt::id_type;

        struct ComponentInfo
        {
            using Callback = ComponentWidgetCallback;
            using SingleCallback = std::function<void(Entity)>;
            using ActionCallback = std::function<Ref<UndoAction>(Entity)>;
            using SelectionActionCallback = std::function<SelectionComponentChange(std::span<const Entity>)>;
            String name;
            Callback widget;
            SelectionActionCallback addToSelection;
            ActionCallback destroy;
            Ref<RetainedUndoActionFactory> undoFactory;
            bool drawsOwnHeader = false;
        };

    public:
        template <class Component> ComponentInfo& RegisterComponent(const ComponentInfo& componentInfo)
        {
            auto hash = entt::type_hash<Component>::value();
            auto [it, res] = m_ComponentInfos[m_CurrentComponentGroup].insert_or_assign(hash, componentInfo);
            CW_ENGINE_ASSERT(res);
            m_OrderedComponentInfos.push_back(std::make_pair(hash, componentInfo));
            if constexpr (ComponentInspectorTraits<Component>::ListedInAddMenu)
                m_ComponentMenu.AddComponent(hash, componentInfo.name, m_CurrentComponentGroup);
            return std::get<ComponentInfo>(*it);
        }

        template <class Component> ComponentInfo& RegisterComponent(const String& name, typename ComponentInfo::SingleCallback widget)
        {
            using Traits = ComponentInspectorTraits<Component>;
            Ref<RetainedUndoActionFactory> undoFactory = Traits::CreateUndoFactory();
            auto wrappedWidget = [widget, undoFactory](Entity primary, const Vector<Entity>& entities) {
                if (entities.size() != 1u)
                {
                    ImGui::Columns(1);
                    ImGui::TextDisabled("This component cannot be edited across multiple entities yet.");
                    ImGui::Columns(2);
                    return;
                }
                Traits::RenderWithUndo(
                  undoFactory, [&widget](Entity entity, const Vector<Entity>&) { widget(entity); }, primary, entities);
            };
            return RegisterComponent<Component>(ComponentInfo{
              name,
              wrappedWidget,
              ComponentSelectionAddAction<Component>,
              ComponentRemoveAction<Component>,
              undoFactory,
              Traits::DrawsOwnHeader,
            });
        }

        template <class Component> ComponentInfo& RegisterComponent(const String& name)
        {
            using Traits = ComponentInspectorTraits<Component>;
            Ref<RetainedUndoActionFactory> undoFactory = Traits::CreateUndoFactory();
            auto widget = [undoFactory](Entity primary, const Vector<Entity>& entities) {
                Traits::RenderWithUndo(undoFactory, ComponentSelectionEditorWidget<Component>, primary, entities);
            };
            return RegisterComponent<Component>(ComponentInfo{
              name,
              widget,
              ComponentSelectionAddAction<Component>,
              ComponentRemoveAction<Component>,
              undoFactory,
              Traits::DrawsOwnHeader,
            });
        }

        void PushComponentGroup(const String& name) { m_CurrentComponentGroup = name; }

        void PopComponentGroup() { m_CurrentComponentGroup.clear(); }

        void Render(Entity primary, const Vector<Entity>& entities);
        void ResetUndoTransactions(bool finishInteraction);

    private:
        bool EntityHasComponent(const entt::registry& registry, const Entity& entity, const ComponentTypeID tid) const
        {
            for (auto [id, storage] : registry.storage())
            {
                if (id == tid)
                    return storage.contains(entity.GetHandle());
            }
            return false;
        }

    private:
        Vector<Pair<ComponentTypeID, ComponentInfo>> m_OrderedComponentInfos;
        Map<String, Map<ComponentTypeID, ComponentInfo>> m_ComponentInfos;
        String m_CurrentComponentGroup;
        ComponentMenuModel m_ComponentMenu;
        String m_ComponentSearch;
        bool m_GrabComponentSearchFocus = true;
        Vector<Entity> m_SelectionScratch;
        Vector<UUID> m_UndoSelection;
        Scene* m_UndoScene = nullptr;
        Ref<ComponentUndoSnapshot<TagComponent>> m_TagSnapshots = CreateRef<ComponentUndoSnapshot<TagComponent>>();
    };

    // Built-in component widgets, defined in ComponentInspector.cpp.
    template <> void ComponentSelectionEditorWidget<TransformComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<CameraComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<LightComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<TextComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<SpriteRendererComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<MeshRendererComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<AnimationComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<Rigidbody2DComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<BoxCollider2DComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<CircleCollider2DComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<Rigidbody3DComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<BoxCollider3DComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<SphereCollider3DComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<CapsuleCollider3DComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<AudioListenerComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<AudioSourceComponent>(Entity primary, const Vector<Entity>& entities);

} // namespace Crowny
