#include "cwepch.h"

#include "Panels/ComponentEditor.h"
#include "Panels/HierarchyPanel.h"

#include <imgui.h>

namespace Crowny
{
    void ComponentEditor::Render()
    {
        Entity entity = HierarchyPanel::GetSelectedEntity();
        entt::registry& registry = SceneManager::GetActiveScene()->m_Registry;
        entt::entity e = entity.m_EntityHandle;

        ImGui::Separator();

        if (entity)
        {
            const String& name = entity.GetName();
            ImGui::Text("%s", name.c_str());
            const String uuid = entity.GetUuid().ToString();
            float uuidLen = ImGui::CalcTextSize(uuid.c_str()).x;
            float nameLen = ImGui::CalcTextSize(name.c_str()).x;
            if (ImGui::GetContentRegionAvail().x > uuidLen + 10.0f + nameLen)
            {
                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetWindowContentRegionWidth() - uuidLen);
                ImGui::Text("%s", uuid.c_str());
            }
        }
        else
        {
            ImGui::Text("Invalid Entity");
        }

        ImGui::Separator();

        if (registry.valid(e))
        {
            ImGui::PushID(entt::to_integral(e));
            for (auto& [component_type_id, ci] : m_OrderedComponentInfos)
            {
                if (EntityHasComponent(registry, e, component_type_id))
                {
                    ImGui::PushID(component_type_id);
                    if (ImGui::Button("-"))
                    {
                        ci.destroy(entity);
                        ImGui::PopID();
                        continue;
                    }
                    else
                    {
                        ImGui::SameLine();
                    }
                    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                    if (ImGui::CollapsingHeader(ci.name.c_str()))
                    {
                        ImGui::Indent(30.f);
                        ImGui::PushID("Widget");
                        ci.widget(entity);
                        ImGui::PopID();
                        ImGui::Unindent(30.f);
                    }
                    ImGui::PopID();
                }
            }
            if (ImGui::Button("+ Add Component"))
                ImGui::OpenPopup("Add Component");

            ImGui::SetNextItemWidth(200);
            if (ImGui::BeginPopup("Add Component"))
            {
                if (ImGui::ArrowButton("<", ImGuiDir_Left))
                    m_CurrentComponentGroup.clear();
                ImGui::SameLine();
                ImGui::Text("%s", m_CurrentComponentGroup.empty() ? "Components" : m_CurrentComponentGroup.c_str());
                ImGui::Separator();

                if (m_CurrentComponentGroup.empty())
                {
                    for (auto& kv : m_ComponentInfos)
                    {
                        if (!kv.first.empty())
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, { 0, 200, 0, 1 });
                            ImGui::PushID(kv.first.c_str());
                            if (ImGui::Selectable(kv.first.c_str(), false, ImGuiSelectableFlags_DontClosePopups))
                                m_CurrentComponentGroup = kv.first;
                            ImGui::PopID();
                            ImGui::PopStyleColor();
                        }
                        else
                        {
                            for (auto& [cId, cInfo] : kv.second)
                            {
                                if (!EntityHasComponent(registry, e, cId))
                                {
                                    ImGui::PushID(cId);
                                    if (ImGui::Selectable(cInfo.name.c_str()))
                                        cInfo.create(entity);

                                    ImGui::PopID();
                                }
                            }
                        }
                    }
                }
                else
                {
                    for (auto& [component_type_id, ci] : m_ComponentInfos[m_CurrentComponentGroup])
                    {
                        if (!EntityHasComponent(registry, e, component_type_id))
                        {
                            ImGui::PushID(component_type_id);
                            if (ImGui::Selectable(ci.name.c_str()))
                                ci.create(entity);

                            ImGui::PopID();
                        }
                    }
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
    }
} // namespace Crowny