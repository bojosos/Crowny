#include "cwepch.h"

#include "Editor/EditorUtils.h"
#include "Editor/ProjectLibrary.h"
#include "Editor/Script/CodeEditor.h"
#include "Editor/Script/ScriptProjectGenerator.h"
#include "Panels/ComponentEditor.h"
#include "Panels/HierarchyPanel.h"

#include "UI/Properties.h"
#include "UI/UIUtils.h"

#include <imgui.h>
#include <regex>

namespace Crowny
{
    static String s_SearchString;
    static String s_DefaultScriptContents;

    // ---------------------------------------------------------------------------
    // Check whether the entity already has a component with the given type ID.
    // Mirrors ComponentEditor::EntityHasComponent but usable from free functions.
    // ---------------------------------------------------------------------------
    static bool HasComponentByID(const entt::registry& registry, const Entity& entity, entt::id_type tid)
    {
        for (auto [id, storage] : registry.storage())
        {
            if (id == tid)
                return storage.contains(entity.GetHandle());
        }
        return false;
    }

    // ---------------------------------------------------------------------------
    // Helper: add a component or script to an entity, eliminating the five
    // duplicate AddScriptComponent call-sites that existed before.
    // ---------------------------------------------------------------------------
    static void AddComponentToEntity(const Ref<Scene>& scene, const Entity& entity, ComponentEditor::ComponentTypeID tid,
                                     const ComponentEditor::ComponentInfo& ci, const String& scriptName = "")
    {
        if (tid == entt::type_hash<MonoScriptComponent>::value())
            scene->AddScriptComponent(entity, scriptName.empty() ? "" : "Sandbox", scriptName, !scriptName.empty());
        else
        {
            auto action = ci.create(const_cast<Entity&>(entity));
            UndoRedo::Get().RegisterAction(action);
        }
    }

    // ---------------------------------------------------------------------------
    // Check whether a specific script is already attached to the entity.
    // ---------------------------------------------------------------------------
    static bool EntityHasScript(const Entity& entity, const String& scriptName)
    {
        if (!entity.HasComponent<MonoScriptComponent>())
            return false;
        const auto& scripts = entity.GetComponent<MonoScriptComponent>().Scripts;
        return std::find_if(scripts.begin(), scripts.end(), [&](const auto& s) { return s.GetTypeName() == scriptName; }) != scripts.end();
    }

    // ---------------------------------------------------------------------------
    // RenderEntityHeader -- entity name + UUID on the same line when space allows.
    // ---------------------------------------------------------------------------
    static void RenderEntityHeader(const Entity& entity)
    {
        if (entity)
        {
            const String& name = entity.GetName();
            ImGui::Text("%s", name.c_str());
            const String uuid = entity.GetUuid().ToString();
            const float uuidLen = ImGui::CalcTextSize(uuid.c_str()).x;
            const float nameLen = ImGui::CalcTextSize(name.c_str()).x;
            if (ImGui::GetContentRegionAvail().x > uuidLen + 10.0f + nameLen)
            {
                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - uuidLen);
                ImGui::Text("%s", uuid.c_str());
            }

            if (entity.HasComponent<PrefabComponent>())
            {
                const auto& pc = entity.GetComponent<PrefabComponent>();
                ImGui::TextColored(ImVec4(0.39f, 0.63f, 1.0f, 1.0f), "Prefab Instance");
                if (!pc.Overrides.empty())
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled("(%zu overrides)", pc.Overrides.size());
                }
            }
        }
        else
        {
            ImGui::Text("Invalid Entity");
        }
    }

    // ---------------------------------------------------------------------------
    // RenderComponents -- iterate all registered components and draw widgets.
    // ---------------------------------------------------------------------------
    static void RenderComponents(const Entity& entity, const entt::registry& registry,
                                 const Vector<Pair<ComponentEditor::ComponentTypeID, ComponentEditor::ComponentInfo>>& orderedInfos)
    {
        // Set up prefab override context if this entity is a prefab instance
        const bool isPrefabInstance = entity.HasComponent<PrefabComponent>();
        PrefabComponent* prefabComp = isPrefabInstance ? &const_cast<Entity&>(entity).GetComponent<PrefabComponent>() : nullptr;

        for (auto& [tid, ci] : orderedInfos)
        {
            if (!HasComponentByID(registry, entity, tid))
                continue;

            // Set the active component name for override tracking
            if (isPrefabInstance)
            {
                PrefabOverrideContext::s_ActivePrefabComponent = prefabComp;
                PrefabOverrideContext::s_ActiveComponentName = ci.name;
            }
            else
            {
                PrefabOverrideContext::s_ActivePrefabComponent = nullptr;
            }

            if (tid == entt::type_hash<MonoScriptComponent>::value())
            {
                // MonoScriptComponent draws its own collapsing headers (one per script).
                ci.widget(const_cast<Entity&>(entity));
                continue;
            }

            ImGui::PushID(tid);
            const bool isTransform = (tid == entt::type_hash<TransformComponent>::value());
            if (!isTransform)
            {
                if (ImGui::Button("-"))
                {
                    auto action = ci.destroy(const_cast<Entity&>(entity));
                    UndoRedo::Get().RegisterAction(action);
                    ImGui::PopID();
                    continue;
                }
                ImGui::SameLine();
            }

            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            if (ImGui::CollapsingHeader(ci.name.c_str()))
            {
                ImGui::Indent(10.f);
                ImGui::PushID("Widget");
                UI::BeginPropertyGrid();
                ci.widget(const_cast<Entity&>(entity));
                UI::EndPropertyGrid();
                ImGui::PopID();
                ImGui::Unindent(10.f);
            }
            ImGui::PopID();
        }

        // Clear the override context
        PrefabOverrideContext::s_ActivePrefabComponent = nullptr;
        PrefabOverrideContext::s_ActiveComponentName.clear();
    }

    // ---------------------------------------------------------------------------
    // CreateNewScript -- "Create new script" flow with solution sync.
    // ---------------------------------------------------------------------------
    static void CreateNewScript(const Ref<Scene>& scene, const Entity& entity, const String& className)
    {
        if (s_DefaultScriptContents.empty())
            s_DefaultScriptContents = FileSystem::ReadTextFile(gApplication->GetWorkingDirectory() / "Resources/Default/DefaultScript.cs");

        String script = StringUtils::Replace(s_DefaultScriptContents, "#NAMESPACE#", Editor::Get().GetProjectPath().filename().string());
        script = StringUtils::Replace(script, "#CLASSNAME#", className);
        Path path = EditorUtils::GetUniquePath(ProjectLibrary::Get().GetAssetFolder() / (className + ".cs"));
        FileSystem::WriteTextFile(path, script);
        ProjectLibrary::Get().Refresh(path);

        // Regenerate VS solution so the new file appears in the IDE.
        Path engineAssemblyPath = gApplication->GetApplicationDesc().EngineAssemblyPath;
        if (engineAssemblyPath.is_relative())
            engineAssemblyPath = gApplication->GetWorkingDirectory() / engineAssemblyPath;
        CodeEditorManager::Get().SyncSolution(GAME_ASSEMBLY, { CROWNY_ASSEMBLY, engineAssemblyPath });

        // The script won't be usable until the assembly is rebuilt.
        // The file watch will trigger auto-rebuild after the debounce.
        scene->AddScriptComponent(entity, "Sandbox", className);
    }

    // ---------------------------------------------------------------------------
    // RenderSearchResults -- flat, filtered list of ALL components + scripts.
    // Already-added items are shown greyed out with "(Added)" suffix.
    // A "Create new script" button appears when the search string is a valid
    // C# class name that doesn't match any existing script.
    // ---------------------------------------------------------------------------
    static void RenderSearchResults(const Ref<Scene>& scene, const Entity& entity, const entt::registry& registry,
                                    const Map<String, Map<ComponentEditor::ComponentTypeID, ComponentEditor::ComponentInfo>>& componentInfos)
    {
        // -- Built-in components --------------------------------------------------
        for (auto& [group, members] : componentInfos)
        {
            for (auto& [tid, ci] : members)
            {
                if (!StringUtils::IsSearchMathing(ci.name, s_SearchString))
                    continue;

                // Skip the generic "Script Component" entry in search -- scripts are
                // listed individually below.
                if (tid == entt::type_hash<MonoScriptComponent>::value())
                    continue;

                const bool alreadyAdded = HasComponentByID(registry, entity, tid);
                if (alreadyAdded)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                    ImGui::PushItemWidth(-1);
                    ImGui::Selectable((ci.name + "  (Added)").c_str(), false, ImGuiSelectableFlags_Disabled);
                    ImGui::PopItemWidth();
                    ImGui::PopStyleColor();
                }
                else
                {
                    ImGui::PushItemWidth(-1);
                    if (ImGui::Button(ci.name.c_str()))
                    {
                        AddComponentToEntity(const_cast<Ref<Scene>&>(scene), entity, tid, ci);
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::PopItemWidth();
                }
            }
        }

        // -- Scripts --------------------------------------------------------------
        const auto& entityBehaviours = ScriptInfoManager::Get().GetEntityBehaviours();
        for (const auto& [name, klass] : entityBehaviours)
        {
            if (klass->GetFullName() == ScriptInfoManager::Get().GetBuiltinClasses().EntityBehaviour->GetFullName())
                continue;
            if (!StringUtils::IsSearchMathing(name, s_SearchString))
                continue;

            const bool exists = EntityHasScript(entity, name);
            if (exists)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                ImGui::PushItemWidth(-1);
                ImGui::Selectable((name + "  (Added)").c_str(), false, ImGuiSelectableFlags_Disabled);
                ImGui::PopItemWidth();
                ImGui::PopStyleColor();
            }
            else
            {
                ImGui::PushItemWidth(-1);
                if (ImGui::Button(name.c_str()))
                {
                    ComponentEditor::ComponentInfo dummy;
                    AddComponentToEntity(const_cast<Ref<Scene>&>(scene), entity, entt::type_hash<MonoScriptComponent>::value(), dummy, name);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopItemWidth();
            }
        }

        // -- "Create new script" button -------------------------------------------
        if (entityBehaviours.find(s_SearchString) == entityBehaviours.end())
        {
            // Only offer creation when the search string is a valid C# class name.
            static const std::regex validClassName("[A-Za-z_][A-Za-z0-9_]*");
            if (std::regex_match(s_SearchString, validClassName))
            {
                ImGui::Separator();
                if (ImGui::Button("Create new script") || Input::IsKeyPressed(Key::Enter))
                {
                    CreateNewScript(scene, entity, s_SearchString);
                    ImGui::CloseCurrentPopup();
                }
            }
        }
    }

    // ---------------------------------------------------------------------------
    // RenderCategoryBrowser -- categorized tree view of all components + scripts.
    // Already-added items are shown greyed out with "(Added)" suffix.
    // ---------------------------------------------------------------------------
    static void RenderCategoryBrowser(const Ref<Scene>& scene, const Entity& entity, const entt::registry& registry,
                                      const Map<String, Map<ComponentEditor::ComponentTypeID, ComponentEditor::ComponentInfo>>& componentInfos)
    {
        for (auto& [group, members] : componentInfos)
        {
            if (group.empty())
            {
                // Uncategorized components -- render directly.
                for (auto& [tid, ci] : members)
                {
                    // Skip the generic "Script Component" entry -- scripts get their own category below.
                    if (tid == entt::type_hash<MonoScriptComponent>::value())
                        continue;

                    const bool alreadyAdded = HasComponentByID(registry, entity, tid);
                    ImGui::PushID(tid);
                    if (alreadyAdded)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                        ImGui::Selectable((ci.name + "  (Added)").c_str(), false, ImGuiSelectableFlags_Disabled);
                        ImGui::PopStyleColor();
                    }
                    else
                    {
                        if (ImGui::Selectable(ci.name.c_str()))
                        {
                            AddComponentToEntity(const_cast<Ref<Scene>&>(scene), entity, tid, ci);
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    ImGui::PopID();
                }
            }
            else
            {
                // Named category -- collapsing tree node.
                if (ImGui::TreeNodeEx(group.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth))
                {
                    for (auto& [tid, ci] : members)
                    {
                        // Skip the generic "Script Component" entry.
                        if (tid == entt::type_hash<MonoScriptComponent>::value())
                            continue;

                        const bool alreadyAdded = HasComponentByID(registry, entity, tid);
                        ImGui::PushID(tid);
                        if (alreadyAdded)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                            ImGui::Selectable((ci.name + "  (Added)").c_str(), false, ImGuiSelectableFlags_Disabled);
                            ImGui::PopStyleColor();
                        }
                        else
                        {
                            if (ImGui::Selectable(ci.name.c_str()))
                            {
                                AddComponentToEntity(const_cast<Ref<Scene>&>(scene), entity, tid, ci);
                                ImGui::CloseCurrentPopup();
                            }
                        }
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }
            }
        }

        // -- "Scripts" category from ScriptInfoManager ----------------------------
        const auto& entityBehaviours = ScriptInfoManager::Get().GetEntityBehaviours();
        bool hasScripts = false;
        for (const auto& [name, klass] : entityBehaviours)
        {
            if (klass->GetFullName() != ScriptInfoManager::Get().GetBuiltinClasses().EntityBehaviour->GetFullName())
            {
                hasScripts = true;
                break;
            }
        }

        if (hasScripts)
        {
            if (ImGui::TreeNodeEx("Scripts", ImGuiTreeNodeFlags_SpanAvailWidth))
            {
                for (const auto& [name, klass] : entityBehaviours)
                {
                    if (klass->GetFullName() == ScriptInfoManager::Get().GetBuiltinClasses().EntityBehaviour->GetFullName())
                        continue;

                    const bool exists = EntityHasScript(entity, name);
                    ImGui::PushID(name.c_str());
                    if (exists)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                        ImGui::Selectable((name + "  (Added)").c_str(), false, ImGuiSelectableFlags_Disabled);
                        ImGui::PopStyleColor();
                    }
                    else
                    {
                        if (ImGui::Selectable(name.c_str()))
                        {
                            ComponentEditor::ComponentInfo dummy;
                            AddComponentToEntity(const_cast<Ref<Scene>&>(scene), entity, entt::type_hash<MonoScriptComponent>::value(), dummy, name);
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
        }
    }

    // ---------------------------------------------------------------------------
    // RenderAddComponentPopup -- the "+ Add Component" popup menu.
    // ---------------------------------------------------------------------------
    static void RenderAddComponentPopup(const Ref<Scene>& scene, const Entity& entity, const entt::registry& registry,
                                        const Map<String, Map<ComponentEditor::ComponentTypeID, ComponentEditor::ComponentInfo>>& componentInfos)
    {
        ImGui::SetNextWindowSizeConstraints(ImVec2(220, 0), ImVec2(400, 400));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));
        if (!ImGui::BeginPopup("Add Component"))
        {
            ImGui::PopStyleVar(2);
            return;
        }

        // Reset search state when the popup first appears.
        if (ImGui::GetCurrentWindow()->Appearing)
            s_SearchString.clear();

        static bool s_GrabFocus = true;
        if (ImGui::GetCurrentWindow()->Appearing)
            s_GrabFocus = true;

        UIUtils::SearchWidget(s_SearchString, "Search...", &s_GrabFocus);

        if (!s_SearchString.empty())
        {
            RenderSearchResults(scene, entity, registry, componentInfos);
        }
        else
        {
            ImGui::Separator();
            RenderCategoryBrowser(scene, entity, registry, componentInfos);
        }

        ImGui::EndPopup();
        ImGui::PopStyleVar(2);
    }

    // ===========================================================================
    // ComponentEditor::Render
    // ===========================================================================
    void ComponentEditor::Render()
    {
        Entity entity = HierarchyPanel::GetSelectedEntity();
        Ref<Scene> scene = gSceneManager->GetActiveScene();
        const entt::registry& registry = scene->m_Registry;

        ImGui::Separator();
        RenderEntityHeader(entity);
        ImGui::Separator();

        if (!entity)
            return;

        ImGui::PushID(entity);
        RenderComponents(entity, registry, m_OrderedComponentInfos);

        if (ImGui::Button("+ Add Component"))
            ImGui::OpenPopup("Add Component");

        RenderAddComponentPopup(scene, entity, registry, m_ComponentInfos);
        ImGui::PopID();
    }

} // namespace Crowny
