#include "cwepch.h"

#include "Panels/HierarchyPanel.h"
#include "UI/UIUtils.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Input/Input.h"
#include "Crowny/Scene/SceneManager.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

namespace Crowny
{
    Entity HierarchyPanel::s_SelectedEntity;

    HierarchyPanel::HierarchyPanel(const String& name, std::function<void(Entity)> callback)
      : ImGuiPanel(name), m_SelectionChanged(callback)
    {
    }

    void HierarchyPanel::DisplayPopup(Entity e)
    {
        if (ImGui::MenuItem("New Entity"))
            CreateEmptyEntity(e);

        if (ImGui::MenuItem("Rename"))
        {
            m_Renaming = e;
            m_RenamingString = e.GetName();
        }

        if (ImGui::MenuItem("Delete"))
        {
            m_DeferedActions.push_back([e]() mutable { e.Destroy(); }); // not sure if neccessary
            HierarchyPanel::s_SelectedEntity = SceneManager::GetActiveScene()->GetRootEntity();
            m_SelectionChanged(s_SelectedEntity);
        }

        if (ImGui::BeginMenu("Create"))
        {
            if (ImGui::MenuItem("Camera"))
                CreateEntityWith<CameraComponent>(e, "Camera");

            if (ImGui::MenuItem("Audio Source"))
                CreateEntityWith<AudioSourceComponent>(e, "Audio Source");

            if (ImGui::MenuItem("Light"))
            {
                // Currently is impossible to do
                // m_NewEntityParent = activeScene->GetRootEntity();
            }

            if (ImGui::MenuItem("Sphere"))
            {
                // Currently is impossible to do
                // m_NewEntityParent = activeScene->GetRootEntity();
            }

            ImGui::EndMenu();
        }
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
        auto& tc = e.GetComponent<TagComponent>();
        auto& rc = e.GetComponent<RelationshipComponent>();

        // ImGui::SetCursorPosX(ImGui::GetCursorPos().x + ImGui::GetStyle().FramePadding.x);
        ImGui::SetKeyboardFocusHere();
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (ImGui::InputText("##renaming", &m_RenamingString,
                             ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
        {
            m_Renaming.GetComponent<TagComponent>().Tag = m_RenamingString;
            m_Renaming = {};
        }

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left | ImGuiMouseButton_Right) && !ImGui::IsItemClicked())
        {
            m_Renaming.GetComponent<TagComponent>().Tag = m_RenamingString;
            m_Renaming = {};
        }
    }

    void HierarchyPanel::DisplayTreeNode(Entity entity)
    {
        auto& tc = entity.GetComponent<TagComponent>();
        auto& rc = entity.GetComponent<RelationshipComponent>();
        String name = tc.Tag.empty() ? "Entity" : tc.Tag.c_str();

        ImGuiTreeNodeFlags selected =
          (m_SelectedItems.find(entity) != m_SelectedItems.end()) ? ImGuiTreeNodeFlags_Selected : 0;
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
                                   ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap |
                                   ImGuiTreeNodeFlags_OpenOnDoubleClick;

        bool open = true;
        if (entity == m_Renaming)
        {
            Rename(entity);
            for (auto& c : rc.Children)
                DisplayTree(c);
        }
        else
        {
            if (m_NewOpenEntity == entity)
            {
                ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                m_NewOpenEntity = {};
            }
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (m_PreserveHierarchy && m_Hierarchy.find(entity.GetUuid()) != m_Hierarchy.end())
                ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            open = ImGui::TreeNodeEx(name.c_str(), selected | flags);

            if (ImGui::BeginDragDropSource())
            {
                UIUtils::SetEntityPayload(entity);
                ImGui::Text("%s", name.c_str());
                ImGui::EndDragDropSource();
            }

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = UIUtils::AcceptEntityPayload())
                {
                    Entity entity = UIUtils::GetEntityFromPayload(payload);
                    entity.SetParent(entity);
                }
                ImGui::EndDragDropTarget();
            }

            if (ImGui::BeginPopupContextItem())
            {
                DisplayPopup(entity);
                ImGui::EndPopup();
            }

            if (Input::IsMouseButtonUp(Mouse::ButtonLeft) && ImGui::IsItemHovered())
                Select(entity);

            if (open)
            {
                m_Hierarchy.insert(entity.GetUuid());
                for (auto c : rc.Children)
                    DisplayTree(c);

                ImGui::TreePop();
            }
        }
    }

    void HierarchyPanel::DisplayLeafNode(Entity e)
    {
        auto& tc = e.GetComponent<TagComponent>();

        String name = tc.Tag.empty() ? "Entity" : tc.Tag.c_str();

        ImGuiTreeNodeFlags selected =
          (m_SelectedItems.find(e) != m_SelectedItems.end()) ? ImGuiTreeNodeFlags_Selected : 0;
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
                                   ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap |
                                   ImGuiTreeNodeFlags_Leaf;

        if (e == m_Renaming)
            Rename(e);
        else
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TreeNodeEx(name.c_str(), flags | selected);

            if (ImGui::BeginDragDropSource())
            {
                // m_SelectedItems.size()
                uint32_t tmp = (uint32_t)e.GetHandle();
                ImGui::SetDragDropPayload("Entity_ID", &tmp, sizeof(uint32_t));
                ImGui::Text("%s", name.c_str());
                ImGui::EndDragDropSource();
            }

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Entity_ID"))
                {
                    CW_ENGINE_ASSERT(payload->DataSize == sizeof(uint32_t));
                    uint32_t id = *(const uint32_t*)payload->Data;
                    Entity((entt::entity)id, SceneManager::GetActiveScene().get()).SetParent(e);
                }
                ImGui::EndDragDropTarget();
            }

            if (Input::IsMouseButtonUp(Mouse::ButtonLeft) && ImGui::IsItemHovered())
                Select(e);

            if (ImGui::BeginPopupContextItem())
            {
                DisplayPopup(e);
                ImGui::EndPopup();
            }
        }
    }
    static int var = 1;
    void HierarchyPanel::DisplayTree(Entity e)
    {
        if (!e.IsValid())
            return;

        auto& rc = e.GetComponent<RelationshipComponent>();

        ImGui::AlignTextToFramePadding();

        ImGui::PushID((int32_t)e.GetHandle());

        if (!rc.Children.empty())
            DisplayTreeNode(e);
        else
            DisplayLeafNode(e);

        ImGui::PopID();
    }

    void HierarchyPanel::CreateEmptyEntity(Entity parent)
    {
        m_DeferedActions.push_back([this, parent]() mutable {
            auto activeScene = SceneManager::GetActiveScene();
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

    void HierarchyPanel::Update()
    {
        for (auto action : m_DeferedActions)
            action();
        m_DeferedActions.clear();

        Scene& activeScene = *SceneManager::GetActiveScene().get(); // Oh god

        if (m_Focused && s_SelectedEntity)
        {
            bool ctrl = Input::IsKeyPressed(Key::LeftControl);
            if (ctrl && Input::IsKeyDown(Key::D)) // Duplicate entities
            {
                if (s_SelectedEntity.GetParent())
                    activeScene.DuplicateEntity(s_SelectedEntity).SetParent(s_SelectedEntity.GetParent());
                else
                    activeScene.DuplicateEntity(s_SelectedEntity).SetParent(s_SelectedEntity.GetParent());
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

            if (Input::IsKeyDown(Key::Delete)) // Deleting
                s_SelectedEntity.Destroy();
        }
    }

    void HierarchyPanel::Render()
    {
        UI::ScopedStyle windowPadding(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        BeginPanel();
        if (!m_PreserveHierarchy)
            m_Hierarchy.clear();
        Ref<Scene> activeScene = SceneManager::GetActiveScene();
        if (ImGui::BeginPopupContextWindow(nullptr,
                                           ImGuiPopupFlags_NoOpenOverExistingPopup | ImGuiPopupFlags_MouseButtonRight))
        {
            if (ImGui::MenuItem("New Entity"))
                CreateEmptyEntity(activeScene->GetRootEntity());

            if (ImGui::BeginMenu("Create"))
            {
                if (ImGui::MenuItem("Camera"))
                    CreateEntityWith<CameraComponent>(activeScene->GetRootEntity(), "Camera");

                if (ImGui::MenuItem("Audio Source"))
                    CreateEntityWith<AudioSourceComponent>(activeScene->GetRootEntity(), "Audio Source");

                if (ImGui::MenuItem("Light"))
                {
                }

                if (ImGui::MenuItem("Sphere"))
                {
                }

                ImGui::EndMenu();
            }
            ImGui::EndPopup();
        }

        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            UI::ScopedColor tableBg(ImGuiCol_ChildBg, IM_COL32(26, 26, 26, 255));
            UI::ScopedStyle innerSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
            UI::ScopedStyle framePadding(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 2.0f));
            UI::ScopedStyle cellPadding(ImGuiStyleVar_CellPadding, ImVec2(0.0f, 0.0f));

            static ImGuiTableFlags flags = ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_Resizable |
                                           ImGuiTableFlags_Reorderable | ImGuiTableFlags_ScrollY;

            if (ImGui::BeginTable("3ways", 1, flags))
            {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
                // ImGui::TableHeadersRow();
                DisplayTree(activeScene->GetRootEntity());
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
            CW_ENGINE_INFO("{0}{1}: {2}", tabs, entity.GetName(),
                           entity.GetParent() ? entity.GetParent().GetName() : "");
            tabs += "\t";
            for (auto child : entity.GetComponent<RelationshipComponent>().Children)
                traverse(child);
            tabs = tabs.substr(0, tabs.size() - 1);
        };

        traverse(s_SelectedEntity);
    }
#endif

} // namespace Crowny
