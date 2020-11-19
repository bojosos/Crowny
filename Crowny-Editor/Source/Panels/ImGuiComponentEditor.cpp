#include "cwepch.h"

#include "Panels/ImGuiComponentEditor.h"
#include "Panels/ImGuiHierarchyPanel.h"

namespace Crowny
{
    void ImGuiComponentEditor::Render()
    {
        Entity entity = ImGuiHierarchyPanel::GetSelectedEntity();
        entt::registry& registry = SceneManager::GetActiveScene()->m_Registry;
        entt::entity e = entity.m_EntityHandle;

        ImGui::Separator();

        if (e != entt::null) {
            ImGui::Text(entity.GetComponent<TagComponent>().Tag.c_str());
        }
        else {
            ImGui::Text("Invalid Entity");
        }

        ImGui::Separator();

        if (registry.valid(e)) {
            ImGui::PushID(entt::to_integral(e));
            std::map<ComponentTypeID, ComponentInfo> has_not;
            for (auto& [component_type_id, ci] : m_ComponentInfos) {
                if (EntityHasComponent(registry, e, component_type_id)) {
                    ImGui::PushID(component_type_id);
                    if (ImGui::Button("-")) {
                        ci.destroy(entity);
                        ImGui::PopID();
                        continue; // early out to prevent access to deleted data
                    }
                    else {
                        ImGui::SameLine();
                    }
                    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                    if (ImGui::CollapsingHeader(ci.name.c_str())) {
                        ImGui::Indent(30.f);
                        ImGui::PushID("Widget");
                        ci.widget(entity);
                        ImGui::PopID();
                        ImGui::Unindent(30.f);
                    }
                    ImGui::PopID();
                }
                else {
                    has_not[component_type_id] = ci;
                }
            }

            if (!has_not.empty()) {
                if (ImGui::Button("+ Add Component")) {
                    ImGui::OpenPopup("Add Component");
                }

                ImGui::SetNextItemWidth(200);
                if (ImGui::BeginPopup("Add Component")) {
                    ImGui::Text("Components:  ");
                    ImGui::Separator();

                    for (auto& [component_type_id, ci] : has_not) {
                        ImGui::PushID(component_type_id);
                        if (ImGui::Selectable(ci.name.c_str())) {
                            ci.create(entity);
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndPopup();
                }
            }
            ImGui::PopID();
        }
    }
}