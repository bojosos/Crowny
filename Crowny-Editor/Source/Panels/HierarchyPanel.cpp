#include "cwepch.h"

#include "Panels/HierarchyPanel.h"
#include "UI/UIUtils.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Input/Input.h"
#include "Crowny/Scene/Prefab.h"
#include "Crowny/Scene/SceneManager.h"

#include "Editor/EditorLayer.h"
#include "Editor/PrefabUtils.h"
#include "Editor/ProjectLibrary.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

namespace Crowny
{
    Entity HierarchyPanel::s_SelectedEntity;

    HierarchyPanel::HierarchyPanel(const String& name, std::function<void(Entity)> callback) : ImGuiPanel(name), m_SelectionChanged(callback) {}

    void HierarchyPanel::RenderContextMenu(Entity e)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));
        if (ImGui::MenuItem("New Entity"))
            CreateEmptyEntity(e);

        // Rename is only meaningful when right-clicking a specific non-root entity
        if (e != gSceneManager->GetActiveScene()->GetRootEntity())
        {
            if (ImGui::MenuItem("Rename"))
            {
                m_Renaming = e;
                m_RenamingString = e.GetName();
            }

            if (ImGui::MenuItem("Delete"))
            {
                m_DeferredActions.push_back([e]() mutable { e.Destroy(); });
                HierarchyPanel::s_SelectedEntity = gSceneManager->GetActiveScene()->GetRootEntity();
                m_SelectionChanged(s_SelectedEntity);
            }
        }

        if (e != gSceneManager->GetActiveScene()->GetRootEntity())
        {
            ImGui::Separator();
            if (ImGui::MenuItem("Create Prefab"))
            {
                m_DeferredActions.push_back([e]() mutable { PrefabUtils::CreatePrefabFromEntity(e); });
            }

            if (e.HasComponent<PrefabComponent>())
            {
                if (ImGui::MenuItem("Apply to Prefab"))
                    m_DeferredActions.push_back([e]() mutable { PrefabUtils::ApplyInstanceToPrefab(e); });
                if (ImGui::MenuItem("Revert Prefab Instance"))
                    m_DeferredActions.push_back([e]() mutable { PrefabUtils::RevertInstance(e); });
                if (ImGui::MenuItem("Unlink Prefab"))
                    m_DeferredActions.push_back([e]() mutable { PrefabUtils::UnlinkPrefab(e); });
            }
        }

        if (ImGui::BeginMenu("Create"))
        {
            if (ImGui::MenuItem("Camera"))
                CreateEntityWith<CameraComponent>(e, "Camera");

            if (ImGui::MenuItem("Audio Source"))
                CreateEntityWith<AudioSourceComponent>(e, "Audio Source");

            // TODO: Light and Sphere creation not yet implemented
            ImGui::EndMenu();
        }
        ImGui::PopStyleVar();
    }

    void HierarchyPanel::Select(Entity e)
    {
        if (!m_SelectedItems.empty() && Input::IsKeyPressed(Key::LeftControl))
        {
            if (m_SelectedItems.find(e) == m_SelectedItems.end())
                m_SelectedItems.insert(e);
            else
            {
                m_SelectedItems.erase(e);
                if (m_SelectedItems.empty())
                {
                    s_SelectedEntity = {};
                    m_SelectionChanged(s_SelectedEntity);
                }
            }
        }
        else
        {
            m_SelectedItems.clear();
            m_SelectedItems.insert(e);
            HierarchyPanel::s_SelectedEntity = e;
            m_SelectionChanged(s_SelectedEntity);
        }
    }

    void HierarchyPanel::Rename(Entity e)
    {
        ImGui::SetKeyboardFocusHere();
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImVec2 framePadding = ImGui::GetStyle().FramePadding;
        framePadding.x += ImGui::GetCursorPosX() + 4.0f;
        UI::ScopedStyle style(ImGuiStyleVar_FramePadding, framePadding);

        bool confirmed = ImGui::InputText("##renaming", &m_RenamingString, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
        bool deactivated = ImGui::IsItemDeactivated() && !ImGui::IsItemActive();

        if (confirmed || deactivated)
        {
            if (!Input::IsKeyPressed(Key::Escape))
                m_Renaming.GetComponent<TagComponent>().Tag = m_RenamingString;
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
        const auto& tc = entity.GetComponent<TagComponent>();
        const auto& rc = entity.GetComponent<RelationshipComponent>();
        const String name = tc.Tag.empty() ? "Entity" : tc.Tag.c_str();

        ImGuiTreeNodeFlags selected = (m_SelectedItems.find(entity) != m_SelectedItems.end()) ? ImGuiTreeNodeFlags_Selected : 0;
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowOverlap;

        if (hasChildren)
            flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        else
            flags |= ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_Leaf;

        if (entity == m_Renaming)
        {
            ImGui::PushID(name.c_str());
            ImVec2 framePadding = ImGui::GetStyle().FramePadding;
            framePadding.x = ImGui::GetCursorPosX();
            UI::ScopedStyle style(ImGuiStyleVar_FramePadding, framePadding);
            Rename(entity);
            if (hasChildren)
            {
                for (auto& c : rc.Children)
                    DisplayTree(c);
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

        bool open = ImGui::TreeNodeEx(name.c_str(), selected | flags);

        if (isPrefabInstance)
            ImGui::PopStyleColor();

        // Selected-row accent indicator: 2px amber column on the left edge.
        if (selected)
        {
            const ImVec2 rowMin = ImGui::GetItemRectMin();
            const ImVec2 rowMax = ImGui::GetItemRectMax();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            // Anchor the stripe to the table column's left edge so nesting indent doesn't shift it.
            float leftX = ImGui::GetCurrentTable() ? ImGui::GetCurrentTable()->WorkRect.Min.x : rowMin.x;
            drawList->AddRectFilled(ImVec2(leftX, rowMin.y), ImVec2(leftX + 2.0f, rowMax.y), UI::Colors::Accent);
        }

        // Drag source
        if (ImGui::BeginDragDropSource())
        {
            UIUtils::SetEntityPayload(entity);
            ImGui::Text("%s", name.c_str());
            ImGui::EndDragDropSource();
        }

        // Drop target
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = UIUtils::AcceptEntityPayload())
            {
                Entity payloadEntity = UIUtils::GetEntityFromPayload(payload);
                payloadEntity.SetParent(entity);
                m_NewOpenEntity = payloadEntity;
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
                        s_SelectedEntity = instance;
                        m_SelectionChanged(s_SelectedEntity);
                        m_SelectedItems.clear();
                        m_SelectedItems.insert(instance);
                    }
                });
            }

            ImGui::EndDragDropTarget();
        }

        // Context menu
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
        if (ImGui::BeginPopupContextItem())
        {
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
            auto activeScene = gSceneManager->GetActiveScene();
            Entity newEntity = activeScene->CreateEntity("New Entity");
            parent.AddChild(newEntity);
            HierarchyPanel::s_SelectedEntity = newEntity;
            m_SelectionChanged(s_SelectedEntity);
            m_SelectedItems.clear();
            m_SelectedItems.insert(newEntity);

            m_NewOpenEntity = parent;
        });
    }

    const UnorderedSet<Crowny::UUID>& HierarchyPanel::GetSerializableHierarchy() { return m_Hierarchy; }

    bool HierarchyPanel::MatchesSearchFilter(Entity e) const
    {
        if (m_SearchFilter.empty())
            return true;

        const String& entityName = e.GetName();
        // Case-insensitive substring match
        String lowerName = entityName;
        String lowerFilter = m_SearchFilter;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
        return lowerName.find(lowerFilter) != String::npos;
    }

    void HierarchyPanel::CollectMatchingEntities(Entity e, Vector<Entity>& results) const
    {
        if (!e.IsValid())
            return;

        // Skip the root entity itself from results, but search its children
        const auto& rc = e.GetComponent<RelationshipComponent>();
        const Entity root = gSceneManager->GetActiveScene()->GetRootEntity();
        if (e != root && MatchesSearchFilter(e))
            results.push_back(e);

        for (auto child : rc.Children)
            CollectMatchingEntities(child, results);
    }

    String HierarchyPanel::BuildParentPath(Entity e) const
    {
        String path;
        const Entity root = gSceneManager->GetActiveScene()->GetRootEntity();
        Entity parent = e.GetParent();
        while (parent && parent != root)
        {
            if (path.empty())
                path = parent.GetName();
            else
                path = parent.GetName() + " / " + path;
            parent = parent.GetParent();
        }
        return path;
    }

    void HierarchyPanel::RenderSearchResults()
    {
        Vector<Entity> matches;
        CollectMatchingEntities(gSceneManager->GetActiveScene()->GetRootEntity(), matches);

        for (auto& entity : matches)
        {
            if (!entity.IsValid())
                continue;

            const String name = entity.GetName().empty() ? "Entity" : entity.GetName();
            const String parentPath = BuildParentPath(entity);

            ImGui::PushID((int32_t)entity.GetHandle());

            ImGuiTreeNodeFlags selected = (m_SelectedItems.find(entity) != m_SelectedItems.end()) ? ImGuiTreeNodeFlags_Selected : 0;
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding |
                                       ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_Leaf;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            if (entity == m_Renaming)
            {
                Rename(entity);
            }
            else
            {
                ImGui::TreeNodeEx(name.c_str(), flags | selected);

                // Show parent path in grey after the name
                if (!parentPath.empty())
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled("  (%s)", parentPath.c_str());
                }
            }

            // Drag source
            if (ImGui::BeginDragDropSource())
            {
                UIUtils::SetEntityPayload(entity);
                ImGui::Text("%s", name.c_str());
                ImGui::EndDragDropSource();
            }

            // Drop target
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = UIUtils::AcceptEntityPayload())
                {
                    Entity payloadEntity = UIUtils::GetEntityFromPayload(payload);
                    Entity oldParent = payloadEntity.GetParent();
                    payloadEntity.SetParent(entity);
                    UndoRedo::Get().RegisterAction(CreateRef<EntityReparentAction>(payloadEntity, oldParent, entity));
                    m_NewOpenEntity = payloadEntity;
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
                            s_SelectedEntity = instance;
                            m_SelectionChanged(s_SelectedEntity);
                            m_SelectedItems.clear();
                            m_SelectedItems.insert(instance);
                        }
                    });
                }

                ImGui::EndDragDropTarget();
            }

            // Context menu
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
            if (ImGui::BeginPopupContextItem())
            {
                RenderContextMenu(entity);
                ImGui::EndPopup();
            }
            ImGui::PopStyleVar();

            // Selection
            if (Input::IsMouseButtonUp(Mouse::ButtonLeft) && ImGui::IsItemHovered())
                Select(entity);

            ImGui::PopID();
        }
    }

    void HierarchyPanel::Update()
    {
        for (auto action : m_DeferredActions)
            action();
        m_DeferredActions.clear();

        Scene& activeScene = *gSceneManager->GetActiveScene().get();

        if (m_Focused && s_SelectedEntity && !ImGui::GetIO().WantCaptureKeyboard)
        {
            const bool ctrl = Input::IsKeyPressed(Key::LeftControl);

            if (ctrl && Input::IsKeyDown(Key::D)) // Duplicate all selected entities
            {
                for (const auto& e : m_SelectedItems)
                {
                    if (e.IsValid() && e.GetParent())
                    {
                        m_DeferredActions.push_back([e]() mutable {
                            Scene& scene = *gSceneManager->GetActiveScene().get();
                            scene.DuplicateEntity(e).SetParent(e.GetParent());
                        });
                    }
                }
            }

            if (ctrl && Input::IsKeyDown(Key::N)) // Create empty entity
            {
                Entity newEntity = activeScene.CreateEntity("New Entity");
                s_SelectedEntity.AddChild(newEntity);
                m_NewOpenEntity = s_SelectedEntity;
                s_SelectedEntity = newEntity;
                m_SelectionChanged(newEntity);
                m_SelectedItems.clear();
                m_SelectedItems.insert(s_SelectedEntity);
            }

            if (Input::IsKeyDown(Key::F2)) // Renaming
            {
                m_Renaming = s_SelectedEntity;
                m_RenamingString = s_SelectedEntity.GetName();
            }

            if (Input::IsKeyDown(Key::Delete)) // Delete all selected entities via deferred actions
            {
                for (const auto& e : m_SelectedItems)
                {
                    if (e.IsValid())
                    {
                        Entity copy = e;
                        m_DeferredActions.push_back([copy]() mutable { copy.Destroy(); });
                    }
                }
                m_SelectedItems.clear();
                s_SelectedEntity = gSceneManager->GetActiveScene()->GetRootEntity();
                m_SelectionChanged(s_SelectedEntity);
            }
        }
    }

    void HierarchyPanel::Render()
    {
        UI::ScopedStyle windowPadding(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        BeginPanel();
        if (!m_PreserveHierarchy)
            m_Hierarchy.clear();
        Ref<Scene> activeScene = gSceneManager->GetActiveScene();
        if (!activeScene)
        {
            EndPanel();
            return;
        }

        // Search/filter bar
        {
            UI::ScopedStyle searchPadding(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::InputTextWithHint("##HierarchySearch", "Search entities...", &m_SearchFilter);
        }

        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            UI::ScopedColor tableBg(ImGuiCol_ChildBg, IM_COL32(26, 26, 26, 255));
            UI::ScopedStyle innerSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
            UI::ScopedStyle framePadding(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 2.0f));
            UI::ScopedStyle cellPadding(ImGuiStyleVar_CellPadding, ImVec2(0.0f, 0.0f));

            static ImGuiTableFlags flags =
              ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_ScrollY;

            if (ImGui::BeginTable("3ways", 1, flags))
            {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);

                if (m_SearchFilter.empty())
                    DisplayTree(activeScene->GetRootEntity());
                else
                    RenderSearchResults();

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
                        m_SelectedItems.clear();
                        s_SelectedEntity = {};
                        m_SelectionChanged(s_SelectedEntity);
                    }

                    // Drop target on empty space — reparent to root
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload = UIUtils::AcceptEntityPayload())
                        {
                            Entity payloadEntity = UIUtils::GetEntityFromPayload(payload);
                            payloadEntity.SetParent(activeScene->GetRootEntity());
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
                                    s_SelectedEntity = instance;
                                    m_SelectionChanged(s_SelectedEntity);
                                    m_SelectedItems.clear();
                                    m_SelectedItems.insert(instance);
                                }
                            });
                        }

                        ImGui::EndDragDropTarget();
                    }
                }

                ImGui::EndTable();
            }
        }
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

        traverse(gSceneManager->GetActiveScene()->GetRootEntity());
    }
#endif

} // namespace Crowny
