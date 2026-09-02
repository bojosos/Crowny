#include "cwepch.h"

#include "Editor/PrefabUtils.h"
#include "Editor/ProjectLibrary.h"
#include "Panels/EntityInspector.h"
#include "Panels/ScriptComponentInspector.h"

#include "UI/Properties.h"
#include "UI/UIUtils.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

namespace Crowny
{
    // ---------------------------------------------------------------------------
    // Check whether the entity already has a component with the given type ID.
    // Mirrors EntityInspector::EntityHasComponent but usable from free functions.
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

    static void RegisterSelectionChange(SelectionComponentChange change) { UndoRedo::Get().RegisterAction(change.Action); }

    static void AddComponentToEntities(const Vector<Entity>& entities, const EntityInspector::ComponentInfo& component)
    {
        if (component.addToSelection)
            RegisterSelectionChange(component.addToSelection(entities));
    }

    // ---------------------------------------------------------------------------
    // Check whether a specific script is already attached to the entity.
    // ---------------------------------------------------------------------------
    static bool EntityHasScript(const Entity& entity, const ScriptTypeIdentity& identity)
    {
        Scene* scene = entity ? entity.GetScene() : nullptr;
        return scene != nullptr && scene->HasScriptComponent(entity, identity);
    }

    static String SelectionAddDetail(StringView defaultDetail, size_t presence, size_t selectionSize)
    {
        if (presence == 0u || presence >= selectionSize)
            return String(defaultDetail);

        const size_t missing = selectionSize - presence;
        return String(defaultDetail) + " | Add to " + std::to_string(missing) +
               (missing == 1u ? " missing selected entity" : " missing selected entities");
    }

    static void PushScriptTypeID(const ScriptTypeIdentity& identity)
    {
        ImGui::PushID(identity.Assembly.c_str());
        ImGui::PushID(identity.Namespace.c_str());
        ImGui::PushID(identity.TypeName.c_str());
    }

    static void PopScriptTypeID()
    {
        ImGui::PopID();
        ImGui::PopID();
        ImGui::PopID();
    }

    static const EntityInspector::ComponentInfo* FindComponentInfo(
      const Map<String, Map<EntityInspector::ComponentTypeID, EntityInspector::ComponentInfo>>& componentInfos,
      EntityInspector::ComponentTypeID typeId)
    {
        for (const auto& group : componentInfos)
        {
            const auto& members = group.second;
            const auto member = members.find(typeId);
            if (member != members.end())
                return &member->second;
        }
        return nullptr;
    }

    static void RenderMenuSectionLabel(StringView label)
    {
        ImGui::Dummy(ImVec2(0.0f, 3.0f));
        ImGui::TextDisabled("%.*s", static_cast<int>(label.size()), label.data());
        ImGui::Dummy(ImVec2(0.0f, 1.0f));
    }

    static bool RenderComponentMenuItem(StringView name, StringView detail, bool alreadyAdded)
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
        drawList->AddText(ImVec2(textX, titleY), titleColor, name.data(), name.data() + name.size());
        drawList->AddText(ImVec2(textX, titleY + lineHeight), detailColor, detail.data(), detail.data() + detail.size());
        drawList->PopClipRect();

        if (textX + ImGui::CalcTextSize(name.data(), name.data() + name.size()).x > textMaxX &&
            ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%.*s", static_cast<int>(name.size()), name.data());

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

    // Unity-style compact header: one row holding the editable name (UUID and selection details in the tooltip),
    // followed by the prefab summary when the entity is a prefab instance.
    static void RenderEntityHeader(Entity primary, const Vector<Entity>& entities, const Ref<ComponentUndoSnapshot<TagComponent>>& snapshots)
    {
        if (!primary || entities.empty())
        {
            ImGui::TextDisabled("The selected entities are no longer available.");
            return;
        }

        UndoRedo& undoRedo = UndoRedo::Get();
        if (undoRedo.BeginComponentScope(snapshots))
            snapshots->Capture(entities);

        String name = primary.GetName();
        const bool single = entities.size() == 1u;
        const bool mixed = std::any_of(entities.begin(), entities.end(), [&](Entity entity) { return entity.GetName() != name; });
        if (mixed)
        {
            name.clear();
            ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
        }
        const String hint = single ? String("Name") : fmt::format("{} entities selected", entities.size());
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputTextWithHint("##EntityName", hint.c_str(), &name, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll) ||
            (ImGui::IsItemDeactivatedAfterEdit() && !mixed && name != primary.GetName()))
        {
            for (Entity entity : entities)
                entity.GetComponent<TagComponent>().Tag = name;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) && !ImGui::IsItemActive())
        {
            if (single)
            {
                const UUID::TextBuffer uuid = primary.GetUuid().ToTextBuffer();
                ImGui::SetTooltip("UUID %s", uuid.data());
            }
            else
                ImGui::SetTooltip("%zu entities selected. Primary: %s", entities.size(), primary.GetName().c_str());
        }
        if (mixed)
            ImGui::PopItemFlag();
        snapshots->CompleteFrame();
        undoRedo.EndComponentScope();

        if (single && primary.HasComponent<PrefabComponent>())
            RenderPrefabInstanceHeader(primary);
    }

    static void RenderComponents(Entity primary, const Vector<Entity>& entities, const entt::registry& registry,
                                 const Vector<Pair<EntityInspector::ComponentTypeID, EntityInspector::ComponentInfo>>& orderedInfos)
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

            if (!multiSelection && ci.drawsOwnHeader)
            {
                // The widget draws its own collapsing headers (e.g. one per attached script).
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

    static void RenderSearchResults(const Vector<Entity>& entities, const entt::registry& registry,
                                    const Map<String, Map<EntityInspector::ComponentTypeID, EntityInspector::ComponentInfo>>& componentInfos,
                                    ComponentMenuModel& menu, const String& query)
    {
        const ComponentMenuModel::SearchResults& results = menu.Search(query);
        const Vector<ComponentMenuModel::ComponentEntry>& components = menu.GetComponents();
        const Vector<ComponentMenuModel::ScriptEntry>& scripts = menu.GetScripts();
        const size_t matchCount = results.GetMatchCount();
        if (matchCount == 0)
        {
            ImGui::Dummy(ImVec2(0.0f, 12.0f));
            ImGui::TextWrapped("No components or scripts match \"%s\".", query.c_str());
        }
        else
        {
            ImGui::TextDisabled("%zu %s", matchCount, matchCount == 1 ? "result" : "results");

            if (!results.ComponentIndices.empty())
            {
                RenderMenuSectionLabel("Components");
                ImGui::PushID("ComponentSearchResults");
                for (size_t componentIndex : results.ComponentIndices)
                {
                    const ComponentMenuModel::ComponentEntry& entry = components[componentIndex];
                    const auto typeId = static_cast<EntityInspector::ComponentTypeID>(entry.Id);
                    ImGui::PushID(typeId);
                    const size_t presence =
                      std::count_if(entities.begin(), entities.end(), [&](Entity entity) { return HasComponentByID(registry, entity, typeId); });
                    const bool alreadyAdded = presence == entities.size();
                    const String detail = SelectionAddDetail(entry.Detail, presence, entities.size());
                    if (RenderComponentMenuItem(entry.Name, detail, alreadyAdded))
                    {
                        const EntityInspector::ComponentInfo* info = FindComponentInfo(componentInfos, typeId);
                        if (info != nullptr)
                        {
                            AddComponentToEntities(entities, *info);
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    ImGui::PopID();
                }
                ImGui::PopID();
            }

            if (!results.ScriptIndices.empty())
            {
                RenderMenuSectionLabel("Scripts");
                ImGui::PushID("ScriptSearchResults");
                for (size_t scriptIndex : results.ScriptIndices)
                {
                    const ComponentMenuModel::ScriptEntry& scriptEntry = scripts[scriptIndex];
                    const String& scriptName = scriptEntry.Name;
                    PushScriptTypeID(scriptEntry.Identity);
                    const size_t presence =
                      std::count_if(entities.begin(), entities.end(), [&](Entity entity) { return EntityHasScript(entity, scriptEntry.Identity); });
                    const bool alreadyAdded = presence == entities.size();
                    const String detail = SelectionAddDetail(scriptEntry.Detail, presence, entities.size());
                    if (RenderComponentMenuItem(scriptName, detail, alreadyAdded))
                    {
                        RegisterSelectionChange(AddManagedScriptToSelection(entities, scriptEntry.Identity));
                        ImGui::CloseCurrentPopup();
                    }
                    PopScriptTypeID();
                }
                ImGui::PopID();
            }
        }

        if (ScriptComponentInspector::IsValidScriptClassName(query) && !results.ScriptNameDeclared)
        {
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
            ImGui::Separator();
            RenderMenuSectionLabel("Create a C# script");
            ImGui::TextWrapped("Create %s.cs and attach it to %zu selected %s.", query.c_str(), entities.size(),
                               entities.size() == 1u ? "entity" : "entities");
            const bool createRequested = ImGui::Button(results.CreateScriptLabel.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0.0f));
            if (createRequested || (matchCount == 0 && Input::IsKeyPressed(Key::Enter)))
            {
                if (ScriptComponentInspector::CreateNewScript(entities, query))
                    ImGui::CloseCurrentPopup();
            }
        }
    }

    static void RenderCategoryBrowser(const Vector<Entity>& entities, const entt::registry& registry,
                                      const Map<String, Map<EntityInspector::ComponentTypeID, EntityInspector::ComponentInfo>>& componentInfos,
                                      ComponentMenuModel& menu)
    {
        const Vector<ComponentMenuModel::ComponentEntry>& components = menu.GetComponents();
        StringView currentGroup;
        bool hasCurrentGroup = false;
        ImGui::PushID("ComponentBrowser");
        for (const ComponentMenuModel::ComponentEntry& entry : components)
        {
            if (!hasCurrentGroup || entry.Group != currentGroup)
            {
                currentGroup = entry.Group;
                hasCurrentGroup = true;
                RenderMenuSectionLabel(currentGroup);
            }

            const auto typeId = static_cast<EntityInspector::ComponentTypeID>(entry.Id);
            ImGui::PushID(typeId);
            const size_t presence =
              std::count_if(entities.begin(), entities.end(), [&](Entity entity) { return HasComponentByID(registry, entity, typeId); });
            const bool alreadyAdded = presence == entities.size();
            const String detail = SelectionAddDetail("Built-in component", presence, entities.size());
            if (RenderComponentMenuItem(entry.Name, detail, alreadyAdded))
            {
                const EntityInspector::ComponentInfo* info = FindComponentInfo(componentInfos, typeId);
                if (info != nullptr)
                {
                    AddComponentToEntities(entities, *info);
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::PopID();
        }
        ImGui::PopID();

        const Vector<ComponentMenuModel::ScriptEntry>& scripts = menu.GetScripts();
        const bool hasVisibleScripts =
          std::any_of(scripts.begin(), scripts.end(), [](const ComponentMenuModel::ScriptEntry& entry) { return entry.Visible; });
        if (hasVisibleScripts)
        {
            RenderMenuSectionLabel("Scripts");
            ImGui::PushID("ScriptBrowser");
            for (const ComponentMenuModel::ScriptEntry& script : scripts)
            {
                if (!script.Visible)
                    continue;
                const String& scriptName = script.Name;
                PushScriptTypeID(script.Identity);
                const size_t presence =
                  std::count_if(entities.begin(), entities.end(), [&](Entity entity) { return EntityHasScript(entity, script.Identity); });
                const bool alreadyAdded = presence == entities.size();
                const String detail = SelectionAddDetail(script.Detail, presence, entities.size());
                if (RenderComponentMenuItem(scriptName, detail, alreadyAdded))
                {
                    RegisterSelectionChange(AddManagedScriptToSelection(entities, script.Identity));
                    ImGui::CloseCurrentPopup();
                }
                PopScriptTypeID();
            }
            ImGui::PopID();
        }
    }

    static void RenderAddComponentPopup(Entity primary, const Vector<Entity>& entities, const entt::registry& registry,
                                        const Map<String, Map<EntityInspector::ComponentTypeID, EntityInspector::ComponentInfo>>& componentInfos,
                                        ComponentMenuModel& menu, String& query, bool& grabSearchFocus)
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
        {
            query.clear();
            grabSearchFocus = true;
        }

        ScriptComponentInspector::SynchronizeScriptCatalog(menu);

        ImGui::TextUnformatted("Add component");
        if (entities.size() == 1u)
            ImGui::TextDisabled("Choose a component for %s", primary.GetName().c_str());
        else
            ImGui::TextDisabled("Add to %zu selected entities", entities.size());
        ImGui::Dummy(ImVec2(0.0f, 3.0f));
        UIUtils::SearchWidget(query, "Search components and scripts...", &grabSearchFocus);
        ImGui::Separator();

        ImGui::BeginChild("##AddComponentResults", ImVec2(0.0f, 0.0f), false);
        if (!query.empty())
            RenderSearchResults(entities, registry, componentInfos, menu, query);
        else
            RenderCategoryBrowser(entities, registry, componentInfos, menu);
        ImGui::EndChild();

        ImGui::EndPopup();
        ImGui::PopStyleVar(2);
    }

    void EntityInspector::ResetUndoTransactions(bool finishInteraction)
    {
        ResetUndoFactory(m_TagSnapshots, finishInteraction);
        for (auto& entry : m_OrderedComponentInfos)
            ResetUndoFactory(entry.second.undoFactory, finishInteraction);
        m_UndoSelection.clear();
        m_UndoScene = nullptr;
    }

    void EntityInspector::Render(Entity primary, const Vector<Entity>& selectedEntities)
    {
        Ref<Scene> scene = SceneManager::TryGet()->GetActiveScene();
        if (!scene || !primary || primary.GetScene() != scene.get())
        {
            if (m_UndoScene != nullptr || !m_UndoSelection.empty())
                ResetUndoTransactions(false);
            ImGui::TextDisabled("Select an entity to inspect its components.");
            return;
        }
        m_SelectionScratch.clear();
        m_SelectionScratch.reserve(selectedEntities.size() + 1u);
        for (Entity entity : selectedEntities)
        {
            if (entity && entity.GetScene() == scene.get() &&
                std::find(m_SelectionScratch.begin(), m_SelectionScratch.end(), entity) == m_SelectionScratch.end())
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
            ResetUndoTransactions(m_UndoScene == scene.get());
            m_UndoScene = scene.get();
            m_UndoSelection.reserve(m_SelectionScratch.size());
            for (Entity entity : m_SelectionScratch)
                m_UndoSelection.push_back(entity.GetUuid());
        }
        const entt::registry& registry = scene->m_Registry;

        ImGui::PushID(primary);
        RenderEntityHeader(primary, m_SelectionScratch, m_TagSnapshots);
        ImGui::Separator();

        RenderComponents(primary, m_SelectionScratch, registry, m_OrderedComponentInfos);

        if (ImGui::Button("+  Add component", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
            ImGui::OpenPopup("Add Component");

        RenderAddComponentPopup(primary, m_SelectionScratch, registry, m_ComponentInfos, m_ComponentMenu, m_ComponentSearch,
                                m_GrabComponentSearchFocus);
        ImGui::PopID();
    }

} // namespace Crowny
