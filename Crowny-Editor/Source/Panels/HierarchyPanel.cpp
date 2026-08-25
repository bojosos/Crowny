#include "cwepch.h"

#include "Panels/HierarchyPanel.h"
#include "UI/UIUtils.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Input/Input.h"
#include "Crowny/Scene/Prefab.h"
#include "Crowny/Scene/SceneManager.h"

#include "Editor/PrefabUtils.h"
#include "Editor/ProjectLibrary.h"
#include "Editor/UndoRedo.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

namespace Crowny
{
    HierarchyPanel::HierarchyPanel(const String& name, SelectionChangedCallback callback) : ImGuiPanel(name), m_SelectionChanged(std::move(callback))
    {
    }

    static void DrawSelectedRowAccent(bool selected)
    {
        if (!selected)
            return;

        const ImVec2 rowMin = ImGui::GetItemRectMin();
        const ImVec2 rowMax = ImGui::GetItemRectMax();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float leftX = ImGui::GetCurrentTable() ? ImGui::GetCurrentTable()->WorkRect.Min.x : rowMin.x;
        drawList->AddRectFilled(ImVec2(leftX, rowMin.y), ImVec2(leftX + 2.0f, rowMax.y), UI::Colors::Accent);
    }

    static char FoldSearchCharacter(char character) { return static_cast<char>(std::tolower(static_cast<unsigned char>(character))); }

    void HierarchyPanel::NotifySelectionChanged()
    {
        if (m_SelectionChanged)
            m_SelectionChanged(m_Selection.GetPrimary(), m_Selection.GetAll());
    }

    Vector<Entity> HierarchyPanel::GetTopLevelSelection() const
    {
        Vector<Entity> result;
        for (Entity entity : m_Selection.GetAll())
        {
            if (!entity)
                continue;

            bool hasSelectedAncestor = false;
            for (Entity parent = entity.GetParent(); parent; parent = parent.GetParent())
            {
                if (m_Selection.Contains(parent))
                {
                    hasSelectedAncestor = true;
                    break;
                }
            }
            if (!hasSelectedAncestor)
                result.push_back(entity);
        }
        return result;
    }

    void HierarchyPanel::QueueReparent(const Vector<Entity>& entities, Entity newParent)
    {
        if (entities.empty() || !newParent)
            return;

        Vector<UUID> entityUuids;
        entityUuids.reserve(entities.size());
        for (Entity entity : entities)
        {
            if (entity && entity.GetParent() && entity != newParent && entity.GetScene() == newParent.GetScene() && entity.GetParent() != newParent)
                entityUuids.push_back(entity.GetUuid());
        }
        if (entityUuids.empty())
            return;

        const UUID parentUuid = newParent.GetUuid();
        m_DeferredActions.push_back([entityUuids, parentUuid]() {
            const Ref<Scene> scene = SceneManager::TryGet()->GetActiveScene();
            if (!scene)
                return;
            Entity parent = scene->TryGetEntityFromUuid(parentUuid);
            if (!parent)
                return;

            Ref<UndoActionGroup> actions = CreateRef<UndoActionGroup>(entityUuids.size() == 1u ? "Reparent entity" : "Reparent entities");
            for (const UUID& entityUuid : entityUuids)
            {
                Entity child = scene->TryGetEntityFromUuid(entityUuid);
                if (!child || !child.GetParent() || child.GetParent() == parent)
                    continue;
                Ref<EntityReparentAction> action = CreateRef<EntityReparentAction>(child, child.GetParent(), parent);
                if (child.SetParent(parent))
                    actions->Add(action);
            }
            if (!actions->Empty())
                UndoRedo::Get().RegisterAction(actions);
        });
    }

    void HierarchyPanel::RenderContextMenu(Entity e)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));
        if (ImGui::MenuItem("Create empty child", "Ctrl+N"))
            CreateEmptyEntity(e);

        if (ImGui::BeginMenu("Create child"))
        {
            if (ImGui::MenuItem("Camera"))
                CreateEntityWith<CameraComponent>(e, "Camera");

            if (ImGui::MenuItem("Audio source"))
                CreateEntityWith<AudioSourceComponent>(e, "Audio Source");

            ImGui::EndMenu();
        }

        if (e != SceneManager::TryGet()->GetActiveScene()->GetRootEntity())
        {
            ImGui::Separator();
            if (ImGui::MenuItem("Rename", "F2"))
            {
                m_Renaming = e;
                m_RenamingString = e.GetName();
            }

            if (ImGui::MenuItem("Delete", "Del"))
            {
                const Vector<Entity> entities = m_Selection.Contains(e) ? GetTopLevelSelection() : Vector<Entity>{ e };
                m_DeferredActions.push_back([entities]() mutable {
                    const Ref<Scene> scene = SceneManager::TryGet()->GetActiveScene();
                    if (!scene)
                        return;
                    Ref<UndoActionGroup> actions = CreateRef<UndoActionGroup>(entities.size() == 1u ? "Delete entity" : "Delete entities");
                    for (Entity entity : entities)
                    {
                        if (entity && entity.GetScene() == scene.get())
                        {
                            actions->Add(CreateRef<EntityDeletedAction>(entity, scene));
                            scene->DestroyEntity(entity);
                        }
                    }
                    if (!actions->Empty())
                        UndoRedo::Get().RegisterAction(actions);
                });
                SetSelectedEntity(SceneManager::TryGet()->GetActiveScene()->GetRootEntity());
            }
        }

        if (e != SceneManager::TryGet()->GetActiveScene()->GetRootEntity())
        {
            ImGui::Separator();
            if (ImGui::MenuItem("Create prefab"))
            {
                m_DeferredActions.push_back([e]() mutable { PrefabUtils::CreatePrefabFromEntity(e); });
            }

            if (e.HasComponent<PrefabComponent>())
            {
                if (ImGui::MenuItem("Apply to prefab"))
                    m_DeferredActions.push_back([e]() mutable { PrefabUtils::ApplyInstanceToPrefab(e); });
                if (ImGui::MenuItem("Revert prefab instance"))
                    m_DeferredActions.push_back([e]() mutable { PrefabUtils::RevertInstance(e); });
                if (ImGui::MenuItem("Unlink prefab"))
                    m_DeferredActions.push_back([e]() mutable { PrefabUtils::UnlinkPrefab(e); });
            }
        }
        ImGui::PopStyleVar();
    }

    void HierarchyPanel::Select(Entity e)
    {
        const ImGuiIO& io = ImGui::GetIO();
        m_PendingSelection = e;
        m_PendingSelectionMode = io.KeyShift && io.KeyCtrl ? EntitySelectionMode::AddRange
                                 : io.KeyShift             ? EntitySelectionMode::Range
                                 : io.KeyCtrl              ? EntitySelectionMode::Toggle
                                                           : EntitySelectionMode::Replace;
    }

    void HierarchyPanel::ApplyPendingSelection()
    {
        if (!m_PendingSelection)
            return;
        if (m_Selection.Select(m_PendingSelection, m_PendingSelectionMode, m_VisibleEntities))
            NotifySelectionChanged();
        m_PendingSelection = {};
    }

    void HierarchyPanel::Rename(Entity e)
    {
        if (!ImGui::IsAnyItemActive())
            ImGui::SetKeyboardFocusHere();
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetTreeNodeToLabelSpacing());
        ImGui::SetNextItemWidth(-FLT_MIN);

        const bool confirmed =
          ImGui::InputText("##renaming", &m_RenamingString, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
        const bool deactivated = ImGui::IsItemDeactivated() && !ImGui::IsItemActive();

        if (confirmed || deactivated)
        {
            const bool hasVisibleCharacter = m_RenamingString.find_first_not_of(" \t\r\n") != String::npos;
            if (!Input::IsKeyPressed(Key::Escape) && hasVisibleCharacter)
            {
                TagComponent oldValue = m_Renaming.GetComponent<TagComponent>();
                TagComponent newValue = oldValue;
                newValue.Tag = m_RenamingString;
                if (oldValue.Tag != newValue.Tag)
                {
                    m_Renaming.AddOrReplaceComponent<TagComponent>(newValue);
                    UndoRedo::Get().RegisterAction(CreateRef<ChangeComponentAction<TagComponent>>(m_Renaming, oldValue, newValue));
                }
            }
            m_Renaming.Clear();
            m_RenamingString.clear();
            return;
        }

        if (Input::IsKeyPressed(Key::Escape))
        {
            m_Renaming.Clear();
            m_RenamingString.clear();
        }
    }

    void HierarchyPanel::RenderEntityRow(Entity entity, bool hasChildren)
    {
        m_VisibleEntities.push_back(entity);
        const auto& tc = entity.GetComponent<TagComponent>();
        const auto& rc = entity.GetComponent<RelationshipComponent>();
        const String name = tc.Tag.empty() ? "Entity" : tc.Tag.c_str();

        const bool selected = m_Selection.Contains(entity);
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowOverlap;

        if (hasChildren)
            flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        else
            flags |= ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_Leaf;

        if (entity == m_Renaming)
        {
            ImGui::PushID(name.c_str());
            Rename(entity);
            if (hasChildren)
            {
                ImGui::Indent();
                for (auto& c : rc.Children)
                    DisplayTree(c);
                ImGui::Unindent();
            }
            ImGui::PopID();
            return;
        }

        if (m_NewOpenEntity == entity)
        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            m_NewOpenEntity.Clear();
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        if (hasChildren && m_PreserveHierarchy && m_Hierarchy.find(entity.GetUuid()) != m_Hierarchy.end())
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);

        bool isPrefabInstance = entity.HasComponent<PrefabComponent>();
        if (isPrefabInstance)
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 160, 255, 255));

        bool open = ImGui::TreeNodeEx(name.c_str(), flags | (selected ? ImGuiTreeNodeFlags_Selected : 0));

        if (isPrefabInstance)
            ImGui::PopStyleColor();

        DrawSelectedRowAccent(selected);

        if (isPrefabInstance && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("Prefab instance");

        // Drag source
        if (entity.GetParent() && ImGui::BeginDragDropSource())
        {
            if (!selected)
            {
                m_Selection.Select(entity, EntitySelectionMode::Replace);
                NotifySelectionChanged();
            }
            UIUtils::SetEntityPayload(entity);
            const size_t dragCount = m_Selection.Contains(entity) ? m_Selection.GetAll().size() : 1u;
            if (dragCount == 1u)
                ImGui::TextUnformatted(name.c_str());
            else
                ImGui::Text("%zu entities", dragCount);
            ImGui::EndDragDropSource();
        }

        // Drop target
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = UIUtils::AcceptEntityPayload())
            {
                Entity payloadEntity = UIUtils::GetEntityFromPayload(payload);
                if (m_Selection.Contains(payloadEntity))
                    QueueReparent(GetTopLevelSelection(), entity);
                else
                    QueueReparent({ payloadEntity }, entity);
                m_NewOpenEntity = entity;
            }

            if (const FileEntry* fileEntry = UIUtils::AcceptAssetPayload(AssetType::Prefab))
            {
                Entity dropTarget = entity;
                m_DeferredActions.push_back([fileEntry, dropTarget, this]() mutable {
                    AssetHandle<Asset> asset = ProjectLibrary::Get().Load(fileEntry);
                    AssetHandle<Prefab> prefab = static_asset_cast<Prefab>(asset);
                    Entity instance = PrefabUtils::InstantiatePrefab(prefab, dropTarget);
                    if (instance)
                    {
                        UndoRedo::Get().RegisterAction(CreateRef<EntityCreatedAction>(instance, SceneManager::TryGet()->GetActiveScene()));
                        SetSelectedEntity(instance);
                    }
                });
            }

            ImGui::EndDragDropTarget();
        }

        // Context menu
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
        if (ImGui::BeginPopupContextItem())
        {
            if (!m_Selection.Contains(entity))
            {
                m_Selection.Select(entity, EntitySelectionMode::Replace);
                NotifySelectionChanged();
            }
            RenderContextMenu(entity);
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();

        // Selection
        if (Input::IsMouseButtonUp(Mouse::ButtonLeft) && ImGui::IsItemHovered())
            Select(entity);

        // Children for tree nodes
        if (hasChildren && open)
        {
            m_Hierarchy.insert(entity.GetUuid());
            for (auto c : rc.Children)
                DisplayTree(c);
            ImGui::TreePop();
        }
    }

    void HierarchyPanel::DisplayTree(Entity e)
    {
        if (!e.IsValid())
            return;

        const auto& rc = e.GetComponent<RelationshipComponent>();

        ImGui::AlignTextToFramePadding();
        ImGui::PushID((int32_t)e.GetHandle());

        RenderEntityRow(e, !rc.Children.empty());

        ImGui::PopID();
    }

    void HierarchyPanel::CreateEmptyEntity(Entity parent)
    {
        m_DeferredActions.push_back([this, parent]() mutable {
            auto activeScene = SceneManager::TryGet()->GetActiveScene();
            if (!activeScene || !parent || parent.GetScene() != activeScene.get())
                return;
            Entity newEntity = activeScene->CreateEntity("New Entity");
            parent.AddChild(newEntity);
            UndoRedo::Get().RegisterAction(CreateRef<EntityCreatedAction>(newEntity, activeScene));
            SetSelectedEntity(newEntity);
            m_NewOpenEntity = parent;
        });
    }

    const UnorderedSet<Crowny::UUID>& HierarchyPanel::GetSerializableHierarchy() { return m_Hierarchy; }

    bool HierarchyPanel::MatchesSearchFilter(Entity e) const
    {
        if (m_SearchFilter.empty())
            return true;

        const String& entityName = e.GetName();
        return std::search(entityName.begin(), entityName.end(), m_FoldedSearchFilter.begin(), m_FoldedSearchFilter.end(),
                           [](char character, char foldedFilterCharacter) { return FoldSearchCharacter(character) == foldedFilterCharacter; }) !=
               entityName.end();
    }

    void HierarchyPanel::CollectMatchingEntities(Entity e, Entity root, Vector<Entity>& results) const
    {
        if (!e.IsValid())
            return;

        // Skip the root entity itself from results, but search its children
        const auto& rc = e.GetComponent<RelationshipComponent>();
        if (e != root && MatchesSearchFilter(e))
            results.push_back(e);

        for (auto child : rc.Children)
            CollectMatchingEntities(child, root, results);
    }

    const String& HierarchyPanel::BuildParentPath(Entity e, Entity root)
    {
        m_ParentPathSegments.clear();
        for (Entity parent = e.GetParent(); parent && parent != root; parent = parent.GetParent())
            m_ParentPathSegments.emplace_back(parent.GetName());

        m_ParentPathScratch.clear();
        const auto firstNamed =
          std::find_if(m_ParentPathSegments.begin(), m_ParentPathSegments.end(), [](StringView segment) { return !segment.empty(); });
        if (firstNamed == m_ParentPathSegments.end())
            return m_ParentPathScratch;

        const size_t firstIndex = static_cast<size_t>(std::distance(m_ParentPathSegments.begin(), firstNamed));
        size_t requiredCapacity = (m_ParentPathSegments.size() - firstIndex - 1) * 3;
        for (size_t index = firstIndex; index < m_ParentPathSegments.size(); index++)
            requiredCapacity += m_ParentPathSegments[index].size();
        m_ParentPathScratch.reserve(requiredCapacity);

        for (size_t index = m_ParentPathSegments.size(); index-- > firstIndex;)
        {
            m_ParentPathScratch.append(m_ParentPathSegments[index]);
            if (index != firstIndex)
                m_ParentPathScratch.append(" / ");
        }
        return m_ParentPathScratch;
    }

    void HierarchyPanel::RenderSearchResults(Entity root)
    {
        m_SearchMatches.clear();
        CollectMatchingEntities(root, root, m_SearchMatches);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (m_SearchMatches.empty())
        {
            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            ImGui::TextDisabled("No entities match \"%s\".", m_SearchFilter.c_str());
            return;
        }

        ImGui::TextDisabled("%zu %s", m_SearchMatches.size(), m_SearchMatches.size() == 1 ? "result" : "results");

        for (Entity entity : m_SearchMatches)
        {
            if (!entity.IsValid())
                continue;

            m_VisibleEntities.push_back(entity);

            const String& entityName = entity.GetName();
            const char* name = entityName.empty() ? "Entity" : entityName.c_str();
            const String& parentPath = BuildParentPath(entity, root);

            ImGui::PushID((int32_t)entity.GetHandle());

            const bool selected = m_Selection.Contains(entity);
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding |
                                       ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_Leaf;

            if (entity == m_Renaming)
            {
                Rename(entity);
            }
            else
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                const bool isPrefabInstance = entity.HasComponent<PrefabComponent>();
                if (isPrefabInstance)
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 160, 255, 255));

                ImGui::TreeNodeEx(name, flags | (selected ? ImGuiTreeNodeFlags_Selected : 0));

                if (isPrefabInstance)
                    ImGui::PopStyleColor();

                DrawSelectedRowAccent(selected);

                const ImVec2 rowMin = ImGui::GetItemRectMin();
                const ImVec2 rowMax = ImGui::GetItemRectMax();

                if (!parentPath.empty())
                {
                    const float pathX = rowMin.x + ImGui::GetTreeNodeToLabelSpacing() + ImGui::CalcTextSize(name).x + 10.0f;
                    const float textY = rowMin.y + ImGui::GetStyle().FramePadding.y;
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    drawList->PushClipRect(ImVec2(pathX, rowMin.y), rowMax, true);
                    drawList->AddText(ImVec2(pathX, textY), ImGui::GetColorU32(ImGuiCol_TextDisabled), parentPath.c_str());
                    drawList->PopClipRect();

                    const bool pathIsClipped = pathX + ImGui::CalcTextSize(parentPath.c_str()).x > rowMax.x;
                    if (pathIsClipped && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                        ImGui::SetTooltip("%s", parentPath.c_str());
                }

                if (isPrefabInstance && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) && parentPath.empty())
                    ImGui::SetTooltip("Prefab instance");

                if (ImGui::BeginDragDropSource())
                {
                    if (!selected)
                    {
                        m_Selection.Select(entity, EntitySelectionMode::Replace);
                        NotifySelectionChanged();
                    }
                    UIUtils::SetEntityPayload(entity);
                    const size_t dragCount = m_Selection.Contains(entity) ? m_Selection.GetAll().size() : 1u;
                    if (dragCount == 1u)
                        ImGui::TextUnformatted(name);
                    else
                        ImGui::Text("%zu entities", dragCount);
                    ImGui::EndDragDropSource();
                }

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = UIUtils::AcceptEntityPayload())
                    {
                        Entity payloadEntity = UIUtils::GetEntityFromPayload(payload);
                        if (m_Selection.Contains(payloadEntity))
                            QueueReparent(GetTopLevelSelection(), entity);
                        else
                            QueueReparent({ payloadEntity }, entity);
                        m_NewOpenEntity = entity;
                    }

                    if (const FileEntry* fileEntry = UIUtils::AcceptAssetPayload(AssetType::Prefab))
                    {
                        Entity dropTarget = entity;
                        m_DeferredActions.push_back([fileEntry, dropTarget, this]() mutable {
                            AssetHandle<Asset> asset = ProjectLibrary::Get().Load(fileEntry);
                            AssetHandle<Prefab> prefab = static_asset_cast<Prefab>(asset);
                            Entity instance = PrefabUtils::InstantiatePrefab(prefab, dropTarget);
                            if (instance)
                            {
                                UndoRedo::Get().RegisterAction(CreateRef<EntityCreatedAction>(instance, SceneManager::TryGet()->GetActiveScene()));
                                SetSelectedEntity(instance);
                            }
                        });
                    }

                    ImGui::EndDragDropTarget();
                }

                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
                if (ImGui::BeginPopupContextItem())
                {
                    if (!m_Selection.Contains(entity))
                    {
                        m_Selection.Select(entity, EntitySelectionMode::Replace);
                        NotifySelectionChanged();
                    }
                    RenderContextMenu(entity);
                    ImGui::EndPopup();
                }
                ImGui::PopStyleVar();

                if (Input::IsMouseButtonUp(Mouse::ButtonLeft) && ImGui::IsItemHovered())
                    Select(entity);
            }

            ImGui::PopID();
        }
    }

    void HierarchyPanel::Update()
    {
        if (!m_DeferredActions.empty())
        {
            m_DeferredActionScratch.clear();
            m_DeferredActionScratch.swap(m_DeferredActions);
            const auto recycleScratch = [&]() {
                m_DeferredActionScratch.clear();
                if (m_DeferredActions.empty() && m_DeferredActionScratch.capacity() > m_DeferredActions.capacity())
                    m_DeferredActionScratch.swap(m_DeferredActions);
            };
            try
            {
                for (auto& action : m_DeferredActionScratch)
                    action();
            }
            catch (...)
            {
                recycleScratch();
                throw;
            }
            recycleScratch();
        }

        const Ref<Scene> activeScene = SceneManager::TryGet()->GetActiveScene();
        if (!activeScene)
        {
            if (m_Selection.Clear())
                NotifySelectionChanged();
            return;
        }

        if (m_Selection.Prune(activeScene.get()))
            NotifySelectionChanged();

        Entity selectedEntity = m_Selection.GetPrimary();
        if (m_Focused && selectedEntity && !ImGui::GetIO().WantCaptureKeyboard)
        {
            const bool ctrl = Input::IsKeyPressed(Key::LeftControl) || Input::IsKeyPressed(Key::RightControl);

            if (ctrl && Input::IsKeyDown(Key::A))
            {
                m_Selection.Clear();
                for (Entity entity : m_VisibleEntities)
                    m_Selection.Select(entity, EntitySelectionMode::Add);
                NotifySelectionChanged();
            }

            if (ctrl && Input::IsKeyDown(Key::D)) // Duplicate all selected entities
            {
                const Vector<Entity> entities = GetTopLevelSelection();
                m_DeferredActions.push_back([this, entities]() mutable {
                    const Ref<Scene> scene = SceneManager::TryGet()->GetActiveScene();
                    if (!scene)
                        return;
                    Vector<Entity> duplicates;
                    Ref<UndoActionGroup> actions = CreateRef<UndoActionGroup>(entities.size() == 1u ? "Duplicate entity" : "Duplicate entities");
                    for (Entity entity : entities)
                    {
                        if (entity && entity.GetScene() == scene.get() && entity.GetParent())
                        {
                            Entity duplicate = scene->DuplicateEntity(entity);
                            if (duplicate)
                            {
                                duplicates.push_back(duplicate);
                                actions->Add(CreateRef<EntityCreatedAction>(duplicate, scene));
                            }
                        }
                    }
                    if (!actions->Empty())
                        UndoRedo::Get().RegisterAction(actions);
                    m_Selection.Clear();
                    for (Entity duplicate : duplicates)
                        m_Selection.Select(duplicate, EntitySelectionMode::Toggle);
                    NotifySelectionChanged();
                });
            }

            if (ctrl && Input::IsKeyDown(Key::N)) // Create empty entity
            {
                Entity parent = selectedEntity;
                Entity newEntity = activeScene->CreateEntity("New Entity");
                parent.AddChild(newEntity);
                UndoRedo::Get().RegisterAction(CreateRef<EntityCreatedAction>(newEntity, activeScene));
                m_NewOpenEntity = parent;
                SetSelectedEntity(newEntity);
            }

            if (Input::IsKeyDown(Key::F2) && selectedEntity != activeScene->GetRootEntity()) // Renaming
            {
                m_Renaming = selectedEntity;
                m_RenamingString = selectedEntity.GetName();
            }

            if (Input::IsKeyDown(Key::Delete)) // Delete all selected entities via deferred actions
            {
                const Entity root = activeScene->GetRootEntity();
                const Vector<Entity> entities = GetTopLevelSelection();
                m_DeferredActions.push_back([entities, root]() mutable {
                    const Ref<Scene> scene = SceneManager::TryGet()->GetActiveScene();
                    if (!scene)
                        return;
                    Ref<UndoActionGroup> actions = CreateRef<UndoActionGroup>(entities.size() == 1u ? "Delete entity" : "Delete entities");
                    for (Entity entity : entities)
                    {
                        if (entity && entity != root)
                        {
                            actions->Add(CreateRef<EntityDeletedAction>(entity, scene));
                            scene->DestroyEntity(entity);
                        }
                    }
                    if (!actions->Empty())
                        UndoRedo::Get().RegisterAction(actions);
                });
                SetSelectedEntity(root);
            }
        }
    }

    void HierarchyPanel::Render()
    {
        UI::ScopedStyle windowPadding(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        if (!BeginPanel())
        {
            EndPanel();
            return;
        }
        m_VisibleEntities.clear();
        if (!m_PreserveHierarchy)
            m_Hierarchy.clear();
        Ref<Scene> activeScene = SceneManager::TryGet()->GetActiveScene();
        if (!activeScene)
        {
            ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(12.0f, 12.0f));
            ImGui::TextDisabled("No scene is open.");
            EndPanel();
            return;
        }

        const float toolbarPadding = 8.0f;
        ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(toolbarPadding, toolbarPadding));
        {
            const float createButtonSize = ImGui::GetFrameHeight();
            const float searchWidth =
              std::max(1.0f, ImGui::GetContentRegionAvail().x - createButtonSize - ImGui::GetStyle().ItemSpacing.x - toolbarPadding);
            ImGui::SetNextItemWidth(searchWidth);
            if (UIUtils::SearchWidget(m_SearchFilter, "Search entities..."))
            {
                m_FoldedSearchFilter.resize(m_SearchFilter.size());
                std::transform(m_SearchFilter.begin(), m_SearchFilter.end(), m_FoldedSearchFilter.begin(), FoldSearchCharacter);
            }
            ImGui::SameLine();
            if (ImGui::Button("+##CreateEntity", ImVec2(createButtonSize, createButtonSize)))
                ImGui::OpenPopup("##CreateEntityMenu");
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("Create entity");

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
            if (ImGui::BeginPopup("##CreateEntityMenu"))
            {
                RenderContextMenu(activeScene->GetRootEntity());
                ImGui::EndPopup();
            }
            ImGui::PopStyleVar();
        }
        ImGui::Dummy(ImVec2(0.0f, 3.0f));
        ImGui::Separator();

        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            UI::ScopedStyle innerSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
            UI::ScopedStyle framePadding(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 3.0f));
            UI::ScopedStyle cellPadding(ImGuiStyleVar_CellPadding, ImVec2(4.0f, 0.0f));

            constexpr ImGuiTableFlags flags = ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_ScrollY;

            if (ImGui::BeginTable("##HierarchyTree", 1, flags))
            {
                ImGui::TableSetupColumn("Entity", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHide);

                if (m_SearchFilter.empty())
                    DisplayTree(activeScene->GetRootEntity());
                else
                    RenderSearchResults(activeScene->GetRootEntity());

                // Fill remaining table space with an invisible selectable so right-click works on empty area
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                float remainingHeight = ImGui::GetContentRegionAvail().y;
                if (remainingHeight > 0.0f)
                {
                    ImGui::InvisibleButton("##EmptySpace", ImVec2(-1.0f, remainingHeight));

                    // Right-click on empty space opens context menu
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
                    if (ImGui::BeginPopupContextItem("##HierarchyEmptyContextMenu", ImGuiPopupFlags_MouseButtonRight))
                    {
                        RenderContextMenu(activeScene->GetRootEntity());
                        ImGui::EndPopup();
                    }
                    ImGui::PopStyleVar();

                    // Left-click on empty space deselects
                    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                    {
                        if (m_Selection.Clear())
                            NotifySelectionChanged();
                    }

                    // Drop target on empty space, reparent to root.
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload = UIUtils::AcceptEntityPayload())
                        {
                            Entity payloadEntity = UIUtils::GetEntityFromPayload(payload);
                            const Entity root = activeScene->GetRootEntity();
                            if (m_Selection.Contains(payloadEntity))
                                QueueReparent(GetTopLevelSelection(), root);
                            else
                                QueueReparent({ payloadEntity }, root);
                        }

                        if (const FileEntry* fileEntry = UIUtils::AcceptAssetPayload(AssetType::Prefab))
                        {
                            Entity root = activeScene->GetRootEntity();
                            m_DeferredActions.push_back([fileEntry, root, this]() mutable {
                                AssetHandle<Asset> asset = ProjectLibrary::Get().Load(fileEntry);
                                AssetHandle<Prefab> prefab = static_asset_cast<Prefab>(asset);
                                Entity instance = PrefabUtils::InstantiatePrefab(prefab, root);
                                if (instance)
                                {
                                    UndoRedo::Get().RegisterAction(
                                      CreateRef<EntityCreatedAction>(instance, SceneManager::TryGet()->GetActiveScene()));
                                    SetSelectedEntity(instance);
                                }
                            });
                        }

                        ImGui::EndDragDropTarget();
                    }
                }

                ImGui::EndTable();
            }
        }
        ApplyPendingSelection();
        m_PreserveHierarchy = false;
        EndPanel();
    }

#ifdef CW_DEBUG
    void HierarchyPanel::PrintDebugHierarchy()
    {
        String tabs;
        std::function<void(Entity)> traverse = [&](Entity entity) {
            if (!entity)
                return;
            CW_ENGINE_INFO("{0}{1}: {2}", tabs, entity.GetName(), entity.GetParent() ? entity.GetParent().GetName() : "");
            tabs += "\t";
            for (auto child : entity.GetComponent<RelationshipComponent>().Children)
                traverse(child);
            tabs = tabs.substr(0, tabs.size() - 1);
        };

        traverse(SceneManager::TryGet()->GetActiveScene()->GetRootEntity());
    }
#endif

} // namespace Crowny
