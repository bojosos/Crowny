#include "cwepch.h"

#include "Editor/EditorAssets.h"
#include "Editor/EditorUtils.h"
#include "Editor/PrefabUtils.h"
#include "Editor/ProjectLibrary.h"
#include "Editor/Script/CodeEditor.h"
#include "Editor/Script/ScriptProjectGenerator.h"
#include "Panels/ComponentEditor.h"

#include "UI/Properties.h"
#include "UI/UIUtils.h"

#include <cctype>
#include <imgui.h>

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

    static void ResetUndoFactory(const Ref<RetainedUndoActionFactory>& factory, bool finishInteraction)
    {
        if (!factory)
            return;

        UndoRedo& undoRedo = UndoRedo::Get();
        if (finishInteraction)
            undoRedo.FinishComponentScope(factory);
        else
            undoRedo.CancelComponentScope(factory);
        factory->Reset();
    }

    // ---------------------------------------------------------------------------
    // Helper: add a component or script to an entity, eliminating the five
    // duplicate AddScriptComponent call-sites that existed before.
    // ---------------------------------------------------------------------------
    static bool EntityHasScript(const Entity& entity, const String& scriptName);

    static void AddComponentToEntities(const Ref<Scene>& scene, const Vector<Entity>& entities, const entt::registry& registry,
                                       ComponentEditor::ComponentTypeID tid, const ComponentEditor::ComponentInfo& ci, const String& scriptName = "")
    {
        Ref<UndoActionGroup> actions = CreateRef<UndoActionGroup>(entities.size() == 1u ? "Add component" : "Add components");
        for (Entity entity : entities)
        {
            if (!entity)
                continue;
            if (tid == entt::type_hash<MonoScriptComponent>::value())
            {
                if (scriptName.empty() || !EntityHasScript(entity, scriptName))
                {
                    ChangeScriptComponentAction::State snapshot = ChangeScriptComponentAction::Capture(entity);
                    scene->AddScriptComponent(entity, scriptName.empty() ? "" : "Sandbox", scriptName, !scriptName.empty());
                    actions->Add(CreateRef<ChangeScriptComponentAction>(entity, std::move(snapshot), "Add script"));
                }
            }
            else if (!HasComponentByID(registry, entity, tid))
            {
                actions->Add(ci.create(entity));
            }
        }
        if (!actions->Empty())
            UndoRedo::Get().RegisterAction(actions);
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

    static bool IsValidScriptClassName(const String& value)
    {
        if (value.empty())
            return false;

        const auto isValidFirst = [](unsigned char c) { return std::isalpha(c) != 0 || c == '_'; };
        const auto isValidRest = [](unsigned char c) { return std::isalnum(c) != 0 || c == '_'; };
        return isValidFirst(static_cast<unsigned char>(value.front())) &&
               std::all_of(value.begin() + 1, value.end(), [&](char c) { return isValidRest(static_cast<unsigned char>(c)); });
    }

    struct ComponentMenuEntry
    {
        ComponentEditor::ComponentTypeID TypeId;
        const ComponentEditor::ComponentInfo* Info;
        String Group;
    };

    static Vector<ComponentMenuEntry> GetSortedComponentEntries(
      const Map<String, Map<ComponentEditor::ComponentTypeID, ComponentEditor::ComponentInfo>>& componentInfos)
    {
        Vector<ComponentMenuEntry> entries;
        for (const auto& [group, members] : componentInfos)
        {
            for (const auto& [tid, componentInfo] : members)
            {
                if (tid != entt::type_hash<MonoScriptComponent>::value())
                    entries.push_back({ tid, &componentInfo, group });
            }
        }

        std::sort(entries.begin(), entries.end(), [](const ComponentMenuEntry& lhs, const ComponentMenuEntry& rhs) {
            if (lhs.Group.empty() != rhs.Group.empty())
                return lhs.Group.empty();
            if (lhs.Group != rhs.Group)
                return lhs.Group < rhs.Group;
            return lhs.Info->name < rhs.Info->name;
        });
        return entries;
    }

    static Vector<String> GetSortedScriptNames()
    {
        const ScriptInfoManager& scriptInfo = ScriptInfoManager::Get();
        const auto& entityBehaviours = scriptInfo.GetEntityBehaviours();
        const MonoClass* entityBehaviourBase = scriptInfo.GetBuiltinClasses().EntityBehaviour;

        Vector<String> scriptNames;
        for (const auto& [name, klass] : entityBehaviours)
        {
            if (klass != nullptr && klass != entityBehaviourBase)
                scriptNames.push_back(name);
        }
        std::sort(scriptNames.begin(), scriptNames.end());
        return scriptNames;
    }

    static void RenderMenuSectionLabel(const String& label)
    {
        ImGui::Dummy(ImVec2(0.0f, 3.0f));
        ImGui::TextDisabled("%s", label.c_str());
        ImGui::Dummy(ImVec2(0.0f, 1.0f));
    }

    static bool RenderComponentMenuItem(const String& name, const String& detail, bool alreadyAdded)
    {
        const float lineHeight = ImGui::GetTextLineHeight();
        const float rowHeight = lineHeight * 2.0f + 11.0f;
        const ImGuiSelectableFlags flags = alreadyAdded ? ImGuiSelectableFlags_Disabled : ImGuiSelectableFlags_None;
        const bool clicked = ImGui::Selectable("##ComponentMenuItem", false, flags, ImVec2(0.0f, rowHeight));

        const ImVec2 rowMin = ImGui::GetItemRectMin();
        const ImVec2 rowMax = ImGui::GetItemRectMax();
        const float textX = rowMin.x + ImGui::GetStyle().FramePadding.x + 3.0f;
        const float titleY = rowMin.y + 5.0f;
        const ImU32 titleColor = ImGui::GetColorU32(alreadyAdded ? ImGuiCol_TextDisabled : ImGuiCol_Text);
        const ImU32 detailColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float trailingWidth = alreadyAdded ? ImGui::CalcTextSize("Added").x + 28.0f : ImGui::CalcTextSize("+").x + 24.0f;
        const float textMaxX = rowMax.x - trailingWidth;

        drawList->PushClipRect(rowMin, ImVec2(textMaxX, rowMax.y), true);
        drawList->AddText(ImVec2(textX, titleY), titleColor, name.c_str());
        drawList->AddText(ImVec2(textX, titleY + lineHeight), detailColor, detail.c_str());
        drawList->PopClipRect();

        if (textX + ImGui::CalcTextSize(name.c_str()).x > textMaxX && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", name.c_str());

        if (alreadyAdded)
        {
            const char* badgeText = "Added";
            const ImVec2 badgeTextSize = ImGui::CalcTextSize(badgeText);
            const float badgePaddingX = 6.0f;
            const float badgePaddingY = 2.0f;
            const ImVec2 badgeMax(rowMax.x - 7.0f, rowMin.y + 6.0f + badgeTextSize.y + badgePaddingY * 2.0f);
            const ImVec2 badgeMin(badgeMax.x - badgeTextSize.x - badgePaddingX * 2.0f, rowMin.y + 6.0f);
            drawList->AddRectFilled(badgeMin, badgeMax, ImGui::GetColorU32(ImGuiCol_FrameBg), 3.0f);
            drawList->AddText(badgeMin + ImVec2(badgePaddingX, badgePaddingY), detailColor, badgeText);
        }
        else
        {
            const ImVec2 plusSize = ImGui::CalcTextSize("+");
            drawList->AddText(ImVec2(rowMax.x - plusSize.x - 10.0f, rowMin.y + (rowHeight - plusSize.y) * 0.5f), detailColor, "+");
        }

        return clicked;
    }

    struct PrefabOverrideEntry
    {
        String EntityPath;
        String PropertyPath;
    };

    static void CollectPrefabOverrides(Entity entity, const UUID& prefabAssetUuid, const UUID& prefabRootEntityUuid, bool isInstanceRoot,
                                       const String& entityPath, Vector<PrefabOverrideEntry>& entries)
    {
        if (!entity.HasComponent<PrefabComponent>())
            return;

        const auto& prefabComponent = entity.GetComponent<PrefabComponent>();
        if (prefabComponent.PrefabAssetUuid != prefabAssetUuid)
            return;
        if (!isInstanceRoot && prefabComponent.PrefabEntityUuid == prefabRootEntityUuid)
            return;

        for (const String& propertyPath : prefabComponent.Overrides)
            entries.push_back({ entityPath, propertyPath });

        for (const Entity child : entity.GetChildren())
            CollectPrefabOverrides(child, prefabAssetUuid, prefabRootEntityUuid, false, entityPath + "/" + child.GetName(), entries);
    }

    static void RenderDisabledTooltip(const char* text)
    {
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("%s", text);
    }

    static void RenderPrefabInstanceHeader(Entity entity)
    {
        Entity instanceRoot = PrefabUtils::GetInstanceRoot(entity);
        const auto& rootPrefabComponent = instanceRoot.GetComponent<PrefabComponent>();
        const UUID prefabAssetUuid = rootPrefabComponent.PrefabAssetUuid;
        const UUID prefabRootEntityUuid = rootPrefabComponent.PrefabEntityUuid;
        const Path prefabPath = ProjectLibrary::Get().UuidToPath(prefabAssetUuid);
        const String prefabName = prefabPath.empty() ? "Prefab asset missing" : prefabPath.filename().string();
        const String prefabUuid = prefabAssetUuid.ToString();

        Vector<PrefabOverrideEntry> overrides;
        CollectPrefabOverrides(instanceRoot, prefabAssetUuid, prefabRootEntityUuid, true, instanceRoot.GetName(), overrides);
        std::sort(overrides.begin(), overrides.end(), [](const PrefabOverrideEntry& lhs, const PrefabOverrideEntry& rhs) {
            if (lhs.EntityPath != rhs.EntityPath)
                return lhs.EntityPath < rhs.EntityPath;
            return lhs.PropertyPath < rhs.PropertyPath;
        });

        const bool hasOverrides = !overrides.empty();
        const bool validMapping = PrefabUtils::CanApplyInstanceToPrefab(instanceRoot);
        const bool canChangeOverrides = hasOverrides && validMapping;

        ImGui::Spacing();
        if (ImGui::BeginTable("##PrefabSummary", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
        {
            ImGui::TableSetupColumn("Prefab", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Override count", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(ImVec4(0.39f, 0.63f, 1.0f, 1.0f), "Prefab instance");
            ImGui::TextUnformatted(prefabName.c_str());
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            {
                if (prefabPath.empty())
                    ImGui::SetTooltip("Asset not found\nUUID %s", prefabUuid.c_str());
                else
                    ImGui::SetTooltip("%s\nUUID %s", prefabPath.string().c_str(), prefabUuid.c_str());
            }

            ImGui::TableSetColumnIndex(1);
            const String overrideCount = std::to_string(overrides.size()) + (overrides.size() == 1 ? " override" : " overrides");
            ImGui::TextDisabled("%s", overrideCount.c_str());
            ImGui::EndTable();
        }

        if (hasOverrides)
        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            if (ImGui::TreeNodeEx("Overrides##PrefabOverrides", ImGuiTreeNodeFlags_SpanAvailWidth))
            {
                for (const PrefabOverrideEntry& overrideEntry : overrides)
                {
                    if (overrideEntry.EntityPath == instanceRoot.GetName())
                        ImGui::BulletText("%s", overrideEntry.PropertyPath.c_str());
                    else
                        ImGui::BulletText("%s / %s", overrideEntry.EntityPath.c_str(), overrideEntry.PropertyPath.c_str());
                }
                ImGui::TreePop();
            }
        }
        else
        {
            ImGui::TextDisabled("No local overrides.");
        }

        bool openApplyConfirmation = false;
        bool openRevertConfirmation = false;
        bool openUnlinkConfirmation = false;
        if (ImGui::BeginTable("##PrefabActions", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::BeginDisabled(!canChangeOverrides);
            if (ImGui::Button("Apply instance", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
                openApplyConfirmation = true;
            ImGui::EndDisabled();
            if (!hasOverrides)
                RenderDisabledTooltip("There are no local overrides to apply.");
            else if (!validMapping)
                RenderDisabledTooltip("The prefab asset or its entity mapping could not be resolved.");
            else
                RenderDisabledTooltip("Replace all mapped prefab component values. Hierarchy changes are not included.");

            ImGui::TableSetColumnIndex(1);
            ImGui::BeginDisabled(!canChangeOverrides);
            if (ImGui::Button("Revert overrides", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
                openRevertConfirmation = true;
            ImGui::EndDisabled();
            if (!hasOverrides)
                RenderDisabledTooltip("There are no local overrides to revert.");
            else if (!validMapping)
                RenderDisabledTooltip("The prefab asset or its entity mapping could not be resolved.");

            ImGui::EndTable();
        }

        if (ImGui::Button("Unlink prefab", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
            openUnlinkConfirmation = true;
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("Keep the current values but remove the prefab link.");

        if (openApplyConfirmation)
            ImGui::OpenPopup("Apply instance to prefab?");
        if (openRevertConfirmation)
            ImGui::OpenPopup("Revert prefab overrides?");
        if (openUnlinkConfirmation)
            ImGui::OpenPopup("Unlink prefab instance?");

        if (ImGui::BeginPopupModal("Apply instance to prefab?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped("Replace all mapped component values in %s with values from %s?", prefabName.c_str(), instanceRoot.GetName().c_str());
            ImGui::TextWrapped("The override list is a change summary, not the apply scope.");
            ImGui::TextWrapped("Added, removed, or reparented children are not applied. Other linked instances will refresh.");
            ImGui::Spacing();
            if (ImGui::Button("Replace values", ImVec2(120.0f, 0.0f)))
            {
                PrefabUtils::ApplyInstanceToPrefab(instanceRoot);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("Revert prefab overrides?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped("Discard %zu local %s on %s?", overrides.size(), overrides.size() == 1 ? "override" : "overrides",
                               instanceRoot.GetName().c_str());
            ImGui::TextWrapped("The prefab values will replace them.");
            ImGui::Spacing();
            if (ImGui::Button("Revert", ImVec2(100.0f, 0.0f)))
            {
                PrefabUtils::RevertInstance(instanceRoot);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("Unlink prefab instance?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped("Unlink %s from %s?", instanceRoot.GetName().c_str(), prefabName.c_str());
            ImGui::TextWrapped("This instance will keep its current values. Nested prefab instances will stay linked.");
            ImGui::Spacing();
            if (ImGui::Button("Unlink", ImVec2(100.0f, 0.0f)))
            {
                PrefabUtils::UnlinkPrefab(instanceRoot);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }

    static void RenderEntityHeader(Entity primary, const Vector<Entity>& entities, const Ref<ComponentUndoSnapshot<TagComponent>>& snapshots)
    {
        if (!primary || entities.empty())
        {
            ImGui::TextDisabled("The selected entities are no longer available.");
            return;
        }

        if (entities.size() == 1u)
        {
            ImGui::TextUnformatted(primary.GetName().c_str());
            const String uuid = primary.GetUuid().ToString();
            ImGui::TextDisabled("UUID %s", uuid.c_str());

            if (primary.HasComponent<PrefabComponent>())
                RenderPrefabInstanceHeader(primary);
        }
        else
        {
            ImGui::Text("%zu entities selected", entities.size());
            ImGui::TextDisabled("Primary: %s", primary.GetName().c_str());
        }

        UndoRedo& undoRedo = UndoRedo::Get();
        if (undoRedo.BeginComponentScope(snapshots))
            snapshots->Capture(entities);

        String name = primary.GetName();
        const bool mixed = std::any_of(entities.begin(), entities.end(), [&](Entity entity) { return entity.GetName() != name; });
        if (mixed)
        {
            name.clear();
            ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
        }
        UI::BeginPropertyGrid();
        if (UI::Property("Name", name))
        {
            for (Entity entity : entities)
                entity.GetComponent<TagComponent>().Tag = name;
        }
        UI::EndPropertyGrid();
        if (mixed)
            ImGui::PopItemFlag();
        undoRedo.EndComponentScope();
    }

    static void RenderComponents(Entity primary, const Vector<Entity>& entities, const entt::registry& registry,
                                 const Vector<Pair<ComponentEditor::ComponentTypeID, ComponentEditor::ComponentInfo>>& orderedInfos)
    {
        const bool multiSelection = entities.size() > 1u;
        const bool isPrefabInstance = !multiSelection && primary.HasComponent<PrefabComponent>();
        PrefabComponent* prefabComp = isPrefabInstance ? &primary.GetComponent<PrefabComponent>() : nullptr;

        for (auto& [tid, ci] : orderedInfos)
        {
            const bool commonComponent =
              std::all_of(entities.begin(), entities.end(), [&](Entity entity) { return HasComponentByID(registry, entity, tid); });
            if (!commonComponent)
            {
                ResetUndoFactory(ci.undoFactory, false);
                continue;
            }

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

            if (!multiSelection && tid == entt::type_hash<MonoScriptComponent>::value())
            {
                // MonoScriptComponent draws its own collapsing headers (one per script).
                ci.widget(primary, entities);
                continue;
            }

            ImGui::PushID(tid);
            const bool isTransform = (tid == entt::type_hash<TransformComponent>::value());
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            const bool open = ImGui::CollapsingHeader(ci.name.c_str(), isTransform ? 0 : ImGuiTreeNodeFlags_AllowOverlap);
            bool removeComponent = false;

            if (!isTransform)
            {
                const ImVec2 resumePosition = ImGui::GetCursorScreenPos();
                const ImVec2 headerMin = ImGui::GetItemRectMin();
                const ImVec2 headerMax = ImGui::GetItemRectMax();
                const float actionWidth = ImGui::GetFrameHeight();
                ImGui::SetCursorScreenPos(ImVec2(headerMax.x - actionWidth, headerMin.y));
                ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
                if (ImGui::Button("...##ComponentActions", ImVec2(actionWidth, headerMax.y - headerMin.y)))
                    ImGui::OpenPopup("##ComponentActionsPopup");
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("Component actions");

                if (ImGui::BeginPopup("##ComponentActionsPopup"))
                {
                    ImGui::TextDisabled("%s", ci.name.c_str());
                    ImGui::Separator();
                    if (ImGui::MenuItem("Remove component"))
                    {
                        removeComponent = true;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
                ImGui::SetCursorScreenPos(resumePosition);
            }

            if (removeComponent)
            {
                ResetUndoFactory(ci.undoFactory, true);
                Ref<UndoActionGroup> actions = CreateRef<UndoActionGroup>(entities.size() == 1u ? "Remove component" : "Remove components");
                for (Entity entity : entities)
                    actions->Add(ci.destroy(entity));
                if (!actions->Empty())
                    UndoRedo::Get().RegisterAction(actions);
                ImGui::PopID();
                continue;
            }

            if (open)
            {
                ImGui::Indent(10.f);
                ImGui::PushID("Widget");
                UI::BeginPropertyGrid();
                ci.widget(primary, entities);
                UI::EndPropertyGrid();
                ImGui::PopID();
                ImGui::Unindent(10.f);
            }
            else
                ResetUndoFactory(ci.undoFactory, true);
            ImGui::PopID();
        }

        // Clear the override context
        PrefabOverrideContext::s_ActivePrefabComponent = nullptr;
        PrefabOverrideContext::s_ActiveComponentName.clear();
    }

    static void CreateNewScript(const Ref<Scene>& scene, const Vector<Entity>& entities, const String& className)
    {
        if (s_DefaultScriptContents.empty())
            s_DefaultScriptContents = EditorAssets::GetDefaultScriptTemplate();

        String script = StringUtils::Replace(s_DefaultScriptContents, "#NAMESPACE#", Editor::Get().GetProjectPath().filename().string());
        script = StringUtils::Replace(script, "#CLASSNAME#", className);
        Path path = EditorUtils::GetUniquePath(ProjectLibrary::Get().GetAssetFolder() / (className + ".cs"));
        FileSystem::WriteTextFile(path, script);
        ProjectLibrary::Get().Refresh(path);

        // Regenerate VS solution so the new file appears in the IDE.
        Path engineAssemblyPath = Application::TryGet()->GetApplicationDesc().EngineAssemblyPath;
        if (engineAssemblyPath.is_relative())
            engineAssemblyPath = Application::TryGet()->GetWorkingDirectory() / engineAssemblyPath;
        CodeEditorManager::Get().SyncSolution(GAME_ASSEMBLY, { CROWNY_ASSEMBLY, engineAssemblyPath });

        // The script won't be usable until the assembly is rebuilt.
        // The file watch will trigger auto-rebuild after the debounce.
        for (Entity entity : entities)
            scene->AddScriptComponent(entity, "Sandbox", className);
    }

    static void RenderSearchResults(const Ref<Scene>& scene, Entity primary, const Vector<Entity>& entities, const entt::registry& registry,
                                    const Map<String, Map<ComponentEditor::ComponentTypeID, ComponentEditor::ComponentInfo>>& componentInfos)
    {
        Vector<ComponentMenuEntry> componentMatches;
        for (const ComponentMenuEntry& entry : GetSortedComponentEntries(componentInfos))
        {
            const bool nameMatches = StringUtils::IsSearchMathing(entry.Info->name, s_SearchString);
            const String group = entry.Group.empty() ? "Core" : entry.Group;
            const bool groupMatches = StringUtils::IsSearchMathing(group, s_SearchString);
            if (nameMatches || groupMatches)
                componentMatches.push_back(entry);
        }
        std::sort(componentMatches.begin(), componentMatches.end(),
                  [](const ComponentMenuEntry& lhs, const ComponentMenuEntry& rhs) { return lhs.Info->name < rhs.Info->name; });

        Vector<String> scriptMatches;
        for (const String& scriptName : GetSortedScriptNames())
        {
            if (StringUtils::IsSearchMathing(scriptName, s_SearchString))
                scriptMatches.push_back(scriptName);
        }

        const size_t matchCount = componentMatches.size() + scriptMatches.size();
        if (matchCount == 0)
        {
            ImGui::Dummy(ImVec2(0.0f, 12.0f));
            ImGui::TextWrapped("No components or scripts match \"%s\".", s_SearchString.c_str());
        }
        else
        {
            ImGui::TextDisabled("%zu %s", matchCount, matchCount == 1 ? "result" : "results");

            if (!componentMatches.empty())
            {
                RenderMenuSectionLabel("Components");
                ImGui::PushID("ComponentSearchResults");
                for (const ComponentMenuEntry& entry : componentMatches)
                {
                    ImGui::PushID(entry.TypeId);
                    const size_t presence = std::count_if(entities.begin(), entities.end(),
                                                          [&](Entity entity) { return HasComponentByID(registry, entity, entry.TypeId); });
                    const bool alreadyAdded = presence == entities.size();
                    const String detail =
                      presence == 0u ? (entry.Group.empty() ? "Core component" : entry.Group + " component") : "Add to missing selected entities";
                    if (RenderComponentMenuItem(entry.Info->name, detail, alreadyAdded))
                    {
                        AddComponentToEntities(scene, entities, registry, entry.TypeId, *entry.Info);
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::PopID();
                }
                ImGui::PopID();
            }

            if (!scriptMatches.empty())
            {
                RenderMenuSectionLabel("Scripts");
                ImGui::PushID("ScriptSearchResults");
                for (const String& scriptName : scriptMatches)
                {
                    ImGui::PushID(scriptName.c_str());
                    const bool alreadyAdded =
                      std::all_of(entities.begin(), entities.end(), [&](Entity entity) { return EntityHasScript(entity, scriptName); });
                    if (RenderComponentMenuItem(scriptName, "C# script", alreadyAdded))
                    {
                        ComponentEditor::ComponentInfo dummy;
                        AddComponentToEntities(scene, entities, registry, entt::type_hash<MonoScriptComponent>::value(), dummy, scriptName);
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::PopID();
                }
                ImGui::PopID();
            }
        }

        const auto& entityBehaviours = ScriptInfoManager::Get().GetEntityBehaviours();
        if (IsValidScriptClassName(s_SearchString) && entityBehaviours.find(s_SearchString) == entityBehaviours.end())
        {
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
            ImGui::Separator();
            RenderMenuSectionLabel("Create a C# script");
            ImGui::TextWrapped("Create %s.cs and attach it to %zu selected %s.", s_SearchString.c_str(), entities.size(),
                               entities.size() == 1u ? "entity" : "entities");
            const String createLabel = "Create \"" + s_SearchString + ".cs\"";
            const bool createRequested = ImGui::Button(createLabel.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0.0f));
            if (createRequested || (matchCount == 0 && Input::IsKeyPressed(Key::Enter)))
            {
                CreateNewScript(scene, entities, s_SearchString);
                ImGui::CloseCurrentPopup();
            }
        }
    }

    static void RenderCategoryBrowser(const Ref<Scene>& scene, Entity primary, const Vector<Entity>& entities, const entt::registry& registry,
                                      const Map<String, Map<ComponentEditor::ComponentTypeID, ComponentEditor::ComponentInfo>>& componentInfos)
    {
        const Vector<ComponentMenuEntry> componentEntries = GetSortedComponentEntries(componentInfos);
        String currentGroup;
        bool hasCurrentGroup = false;
        ImGui::PushID("ComponentBrowser");
        for (const ComponentMenuEntry& entry : componentEntries)
        {
            const String group = entry.Group.empty() ? "Core" : entry.Group;
            if (!hasCurrentGroup || group != currentGroup)
            {
                currentGroup = group;
                hasCurrentGroup = true;
                RenderMenuSectionLabel(currentGroup);
            }

            ImGui::PushID(entry.TypeId);
            const bool alreadyAdded =
              std::all_of(entities.begin(), entities.end(), [&](Entity entity) { return HasComponentByID(registry, entity, entry.TypeId); });
            if (RenderComponentMenuItem(entry.Info->name, "Built-in component", alreadyAdded))
            {
                AddComponentToEntities(scene, entities, registry, entry.TypeId, *entry.Info);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
        }
        ImGui::PopID();

        const Vector<String> scriptNames = GetSortedScriptNames();
        if (!scriptNames.empty())
        {
            RenderMenuSectionLabel("Scripts");
            ImGui::PushID("ScriptBrowser");
            for (const String& scriptName : scriptNames)
            {
                ImGui::PushID(scriptName.c_str());
                const bool alreadyAdded =
                  std::all_of(entities.begin(), entities.end(), [&](Entity entity) { return EntityHasScript(entity, scriptName); });
                if (RenderComponentMenuItem(scriptName, "C# script", alreadyAdded))
                {
                    ComponentEditor::ComponentInfo dummy;
                    AddComponentToEntities(scene, entities, registry, entt::type_hash<MonoScriptComponent>::value(), dummy, scriptName);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
            }
            ImGui::PopID();
        }
    }

    static void RenderAddComponentPopup(const Ref<Scene>& scene, Entity primary, const Vector<Entity>& entities, const entt::registry& registry,
                                        const Map<String, Map<ComponentEditor::ComponentTypeID, ComponentEditor::ComponentInfo>>& componentInfos)
    {
        ImGui::SetNextWindowSize(ImVec2(370.0f, 460.0f), ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints(ImVec2(320.0f, 360.0f), ImVec2(480.0f, 620.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 4.0f));
        if (!ImGui::BeginPopup("Add Component"))
        {
            ImGui::PopStyleVar(2);
            return;
        }

        if (ImGui::IsWindowAppearing())
            s_SearchString.clear();

        static bool s_GrabFocus = true;
        if (ImGui::IsWindowAppearing())
            s_GrabFocus = true;

        ImGui::TextUnformatted("Add component");
        if (entities.size() == 1u)
            ImGui::TextDisabled("Choose a component for %s", primary.GetName().c_str());
        else
            ImGui::TextDisabled("Add to %zu selected entities", entities.size());
        ImGui::Dummy(ImVec2(0.0f, 3.0f));
        UIUtils::SearchWidget(s_SearchString, "Search components and scripts...", &s_GrabFocus);
        ImGui::Separator();

        ImGui::BeginChild("##AddComponentResults", ImVec2(0.0f, 0.0f), false);
        if (!s_SearchString.empty())
            RenderSearchResults(scene, primary, entities, registry, componentInfos);
        else
            RenderCategoryBrowser(scene, primary, entities, registry, componentInfos);
        ImGui::EndChild();

        ImGui::EndPopup();
        ImGui::PopStyleVar(2);
    }

    void ComponentEditor::ResetUndoSnapshots(bool finishInteraction)
    {
        ResetUndoFactory(m_TagSnapshots, finishInteraction);
        for (auto& entry : m_OrderedComponentInfos)
            ResetUndoFactory(entry.second.undoFactory, finishInteraction);
        m_UndoSelection.clear();
        m_UndoScene = nullptr;
    }

    void ComponentEditor::Render(Entity primary, const Vector<Entity>& selectedEntities)
    {
        Ref<Scene> scene = SceneManager::TryGet()->GetActiveScene();
        if (!scene || !primary || primary.GetScene() != scene.get())
        {
            if (m_UndoScene != nullptr || !m_UndoSelection.empty())
                ResetUndoSnapshots(false);
            ImGui::TextDisabled("Select an entity to inspect its components.");
            return;
        }
        m_SelectionScratch.clear();
        m_SelectionScratch.reserve(selectedEntities.size() + 1u);
        for (Entity entity : selectedEntities)
        {
            if (entity && entity.GetScene() == scene.get())
                m_SelectionScratch.push_back(entity);
        }
        if (m_SelectionScratch.empty())
            m_SelectionScratch.push_back(primary);
        const auto primaryEntry = std::find(m_SelectionScratch.begin(), m_SelectionScratch.end(), primary);
        if (primaryEntry == m_SelectionScratch.end())
            m_SelectionScratch.insert(m_SelectionScratch.begin(), primary);
        else if (primaryEntry != m_SelectionScratch.begin())
            std::iter_swap(m_SelectionScratch.begin(), primaryEntry);

        const bool selectionChanged = m_UndoScene != scene.get() || m_UndoSelection.size() != m_SelectionScratch.size() ||
                                      !std::equal(m_UndoSelection.begin(), m_UndoSelection.end(), m_SelectionScratch.begin(),
                                                  m_SelectionScratch.end(), [](UUID uuid, Entity entity) { return uuid == entity.GetUuid(); });
        if (selectionChanged)
        {
            ResetUndoSnapshots(m_UndoScene == scene.get());
            m_UndoScene = scene.get();
            m_UndoSelection.reserve(m_SelectionScratch.size());
            for (Entity entity : m_SelectionScratch)
                m_UndoSelection.push_back(entity.GetUuid());
        }
        const entt::registry& registry = scene->m_Registry;

        ImGui::PushID(primary);
        ImGui::Separator();
        RenderEntityHeader(primary, m_SelectionScratch, m_TagSnapshots);
        ImGui::Separator();

        RenderComponents(primary, m_SelectionScratch, registry, m_OrderedComponentInfos);

        if (ImGui::Button("+  Add component", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
            ImGui::OpenPopup("Add Component");

        RenderAddComponentPopup(scene, primary, m_SelectionScratch, registry, m_ComponentInfos);
        ImGui::PopID();
    }

} // namespace Crowny
