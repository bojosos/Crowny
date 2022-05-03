#include "cwepch.h"

#include "Editor/ProjectLibrary.h"
#include "Editor/EditorUtils.h"
#include "Panels/ComponentEditor.h"
#include "Panels/HierarchyPanel.h"

#include "UI/UIUtils.h"

#include <imgui.h>
#include <regex>

namespace Crowny
{
    static String s_SearchString;

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
            ImGui::Text("Invalid Entity");

        ImGui::Separator();

        if (registry.valid(e))
        {
            ImGui::PushID(entt::to_integral(e));
            for (auto& [component_type_id, ci] : m_OrderedComponentInfos)
            {
                if (EntityHasComponent(registry, e, component_type_id))
                {
                    if (component_type_id == entt::type_info<MonoScriptComponent>::id())
                    {
                        // Draw the collapsing headers in the widget itself, since one component can have multiple scripts
                        ci.widget(entity);
                        // ImGui::PopID();
                        continue;
                    }
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
                        UI::BeginPropertyGrid();
                        ci.widget(entity);
                        UI::EndPropertyGrid();
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
                static bool s_GrabFocus = true;
                if (ImGui::GetCurrentWindow()->Appearing)
                {
                    s_GrabFocus = true;
                    s_SearchString.clear();
                }
                UIUtils::SearchWidget(s_SearchString, "Search...", &s_GrabFocus);
                if (!s_SearchString.empty())
                {
                    for (auto& [component_type_id, ci] : m_OrderedComponentInfos)
                    {
                        if (StringUtils::IsSearchMathing(ci.name, s_SearchString))
                        {
                            ImGui::PushItemWidth(-1);
                            if (ImGui::Button(ci.name.c_str()))
                            {
                                ci.create(entity);
                                ImGui::CloseCurrentPopup();
                            }
                        }
                    }
					
					const auto& entityBehaviours = ScriptInfoManager::Get().GetEntityBehaviours();
                    for (auto [name, klass] : entityBehaviours)
                    {
                        bool exists = false;
                        if (entity.HasComponent<MonoScriptComponent>())
                        {
                            const auto& scripts = entity.GetComponent<MonoScriptComponent>().Scripts;
                            if (std::find_if(scripts.begin(), scripts.end(), [&](const auto& script) { return script.GetTypeName() == name; }) != scripts.end())
                                exists = true;
                        }
						if (!exists && StringUtils::IsSearchMathing(name, s_SearchString))
						{
							ImGui::PushItemWidth(-1);
							if (ImGui::Button(name.c_str()))
							{
								if (!entity.HasComponent<MonoScriptComponent>())
								{
									MonoScriptComponent& msc = entity.AddComponent<MonoScriptComponent>(name);
									msc.Scripts[0].OnInitialize(entity);
								}
								else
								{
									auto& scripts = entity.GetComponent<MonoScriptComponent>().Scripts;
									scripts.push_back(MonoScript(name));
									scripts.back().OnInitialize(entity);
								}
								ImGui::CloseCurrentPopup();
							}
						}
                    }
                    if (entityBehaviours.find(s_SearchString) == entityBehaviours.end())
                    {
                        std::regex validClassName = std::regex("[A-Za-z_][A-Za-z0-9_]*"); // This is not technically correct, but it should work just fine
                                                                                          // First this allows for keyword classes (which can be used using @ in front of class names)
                                                                                          // Also not all unicode stuff, but who is going to use Unicode in class names
                        if (std::regex_match(s_SearchString, validClassName))
                        {
                            if (ImGui::Button("Create new script") || Input::IsKeyPressed(Key::Enter)) // Create a new script with the search string
                            {
                                String defaultContents = FileSystem::ReadTextFile("C:\\dev\\Crowny\\Crowny-Editor\\Resources\\Default\\DefaultScript.cs"); // TODO: Don't load this every time
                                String script = StringUtils::Replace(defaultContents, "#NAMESPACE#",
                                                                     Editor::Get().GetProjectPath().filename().string());
                                script = StringUtils::Replace(script, "#CLASSNAME#", s_SearchString);
                                Path path = EditorUtils::GetUniquePath(ProjectLibrary::Get().GetAssetFolder() / (s_SearchString + ".cs"));
                                FileSystem::WriteTextFile(path, script);
                                ProjectLibrary::Get().Refresh(path);
							    ImGui::CloseCurrentPopup();

							    if (!entity.HasComponent<MonoScriptComponent>())
							    {
								    MonoScriptComponent& msc = entity.AddComponent<MonoScriptComponent>(s_SearchString);
								    msc.Scripts[0].OnInitialize(entity);
							    }
							    else
							    {
								    auto& scripts = entity.GetComponent<MonoScriptComponent>().Scripts;
								    bool exists = false;
								    for (auto& script : scripts)
									    if (script.GetTypeName() == s_SearchString)
										    exists = true;
								    if (!exists)
									    scripts.push_back(MonoScript(s_SearchString));
								    scripts.back().OnInitialize(entity);
							    }
                            }
                        }
                    }
                    ImGui::EndPopup();
                    ImGui::PopID();
                    return;
                }
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