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

#include "Crowny/Application/Application.h"
#include "Crowny/Application/EngineRuntime.h"
#include "Crowny/Scripting/Managed/ManagedScripting.h"

#include <cctype>
#include <imgui.h>

namespace Crowny
{
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
    static bool EntityHasScript(const Entity& entity, const ScriptTypeIdentity& identity);

    static void AddComponentToEntities(const Ref<Scene>& scene, const Vector<Entity>& entities, const entt::registry& registry,
                                       ComponentEditor::ComponentTypeID tid, const ComponentEditor::ComponentInfo& ci, const String& scriptName = "")
    {
        Ref<UndoActionGroup> actions = CreateRef<UndoActionGroup>(entities.size() == 1u ? "Add component" : "Add components");
        for (Entity entity : entities)
        {
            if (!entity)
                continue;
            if (tid == entt::type_hash<ManagedScriptComponent>::value())
            {
                const ScriptTypeIdentity identity = scriptName.empty() ? ScriptTypeIdentity{} : ScriptTypeIdentity{ GAME_ASSEMBLY, "Sandbox", scriptName };
                if (!identity.IsValid() || !EntityHasScript(entity, identity))
                {
                    ChangeScriptComponentAction::State snapshot = ChangeScriptComponentAction::Capture(entity);
                    scene->AddScriptComponent(entity, identity, identity.IsValid());
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
    static bool EntityHasScript(const Entity& entity, const ScriptTypeIdentity& identity)
    {
        if (!entity.HasComponent<ManagedScriptComponent>())
            return false;
        const auto& scripts = entity.GetComponent<ManagedScriptComponent>().Scripts;
        return std::find_if(scripts.begin(), scripts.end(), [&](const auto& script) { return script.GetTypeIdentity() == identity; }) != scripts.end();
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

    static void SynchronizeScriptCatalog(ComponentMenuModel& menu)
    {
        Application* application = Application::TryGet();
        const ManagedScripting* managed = application != nullptr ? application->GetRuntime().GetManagedScripting() : nullptr;
        const ScriptCatalog* catalog = managed != nullptr && managed->IsStarted() ? &managed->GetScriptCatalog() : nullptr;
        const uint64_t fingerprint = catalog != nullptr ? catalog->ManifestHash : 1;
        if (menu.HasScriptCatalog(fingerprint))
            return;

        Vector<ComponentMenuModel::ScriptEntry> scripts;
        if (catalog != nullptr)
        {
            scripts.reserve(catalog->Types.size());
            for (const ScriptTypeSchema& type : catalog->Types)
                scripts.push_back({ type.Identity.TypeName, true, type.Identity });
        }
        menu.SetScripts(fingerprint, std::move(scripts));
    }

    static const ComponentEditor::ComponentInfo* FindComponentInfo(
      const Map<String, Map<ComponentEditor::ComponentTypeID, ComponentEditor::ComponentInfo>>& componentInfos,
      ComponentEditor::ComponentTypeID typeId)
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

    static ImVec4 GetComponentMenuSecondaryTextColor()
    {
        ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
        color.w *= 0.72f;
        return color;
    }

    static void RenderMenuSectionLabel(StringView label)
    {
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, GetComponentMenuSecondaryTextColor());
        ImGui::TextUnformatted(label.data(), label.data() + label.size());
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
    }

    enum class ComponentMenuRowAction
    {
        Add,
        Navigate,
        Added,
    };

    static bool RenderComponentMenuRow(StringView name, StringView detail, ComponentMenuRowAction action)
    {
        const float lineHeight = ImGui::GetTextLineHeight();
        const float rowHeight = lineHeight + 14.0f;
        const bool disabled = action == ComponentMenuRowAction::Added;
        const ImGuiSelectableFlags flags = disabled ? ImGuiSelectableFlags_Disabled : ImGuiSelectableFlags_None;
        const bool clicked = ImGui::Selectable("##ComponentMenuItem", false, flags, ImVec2(0.0f, rowHeight));
        const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);

        const ImVec2 rowMin = ImGui::GetItemRectMin();
        const ImVec2 rowMax = ImGui::GetItemRectMax();
        const float textX = rowMin.x + 9.0f;
        const float textY = rowMin.y + (rowHeight - lineHeight) * 0.5f;
        const float trailingWidth = action == ComponentMenuRowAction::Added ? 61.0f : 34.0f;
        const float textMaxX = rowMax.x - trailingWidth;
        const ImVec4 secondaryText = GetComponentMenuSecondaryTextColor();
        const ImU32 titleColor = disabled ? ImGui::GetColorU32(secondaryText) : ImGui::GetColorU32(ImGuiCol_Text);
        const ImU32 detailColor = ImGui::GetColorU32(secondaryText);
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        float titleMaxX = textMaxX;
        if (!detail.empty())
        {
            const float availableWidth = textMaxX - textX;
            const float detailWidth = std::min(ImGui::CalcTextSize(detail.data(), detail.data() + detail.size()).x, availableWidth * 0.42f);
            const float detailX = textMaxX - detailWidth;
            if (detailX - textX >= 110.0f)
            {
                titleMaxX = detailX - 12.0f;
                drawList->PushClipRect(ImVec2(detailX, rowMin.y), ImVec2(textMaxX, rowMax.y), true);
                drawList->AddText(ImVec2(detailX, textY), detailColor, detail.data(), detail.data() + detail.size());
                drawList->PopClipRect();
            }
        }

        drawList->PushClipRect(ImVec2(textX, rowMin.y), ImVec2(titleMaxX, rowMax.y), true);
        drawList->AddText(ImVec2(textX, textY), titleColor, name.data(), name.data() + name.size());
        drawList->PopClipRect();

        if (textX + ImGui::CalcTextSize(name.data(), name.data() + name.size()).x > titleMaxX && hovered)
            ImGui::SetTooltip("%.*s", static_cast<int>(name.size()), name.data());

        if (action == ComponentMenuRowAction::Added)
        {
            const char* badgeText = "Added";
            const ImVec2 badgeTextSize = ImGui::CalcTextSize(badgeText);
            const ImVec2 badgeMax(rowMax.x - 7.0f, rowMin.y + (rowHeight + badgeTextSize.y) * 0.5f + 3.0f);
            const ImVec2 badgeMin(badgeMax.x - badgeTextSize.x - 12.0f, badgeMax.y - badgeTextSize.y - 6.0f);
            drawList->AddRectFilled(badgeMin, badgeMax, ImGui::GetColorU32(ImGuiCol_FrameBg), 3.0f);
            drawList->AddText(badgeMin + ImVec2(6.0f, 3.0f), detailColor, badgeText);
        }
        else if (action == ComponentMenuRowAction::Navigate)
        {
            const ImVec2 center(rowMax.x - 15.0f, (rowMin.y + rowMax.y) * 0.5f);
            drawList->AddTriangleFilled(center + ImVec2(-3.0f, -5.0f), center + ImVec2(-3.0f, 5.0f), center + ImVec2(3.0f, 0.0f),
                                        detailColor);
        }
        else
        {
            const ImVec2 buttonMin(rowMax.x - 29.0f, rowMin.y + 4.0f);
            const ImVec2 buttonMax(rowMax.x - 7.0f, rowMax.y - 4.0f);
            drawList->AddRectFilled(buttonMin, buttonMax, ImGui::GetColorU32(hovered ? ImGuiCol_ButtonHovered : ImGuiCol_FrameBg), 3.0f);
            drawList->AddRect(buttonMin, buttonMax, ImGui::GetColorU32(ImGuiCol_Border), 3.0f);
            const ImVec2 center((buttonMin.x + buttonMax.x) * 0.5f, (buttonMin.y + buttonMax.y) * 0.5f);
            const ImU32 plusColor = ImGui::GetColorU32(ImGuiCol_Text);
            drawList->AddLine(center + ImVec2(-4.0f, 0.0f), center + ImVec2(4.0f, 0.0f), plusColor, 1.5f);
            drawList->AddLine(center + ImVec2(0.0f, -4.0f), center + ImVec2(0.0f, 4.0f), plusColor, 1.5f);
        }

        if (hovered && !disabled)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        return clicked;
    }

    static bool RenderComponentMenuItem(StringView name, StringView detail, bool alreadyAdded)
    {
        return RenderComponentMenuRow(name, detail, alreadyAdded ? ComponentMenuRowAction::Added : ComponentMenuRowAction::Add);
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
            const UUID::TextBuffer uuid = primary.GetUuid().ToTextBuffer();
            ImGui::TextDisabled("UUID %s", uuid.data());

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
        snapshots->CompleteFrame();
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

            if (!multiSelection && tid == entt::type_hash<ManagedScriptComponent>::value())
            {
                // ManagedScriptComponent draws its own collapsing headers (one per script).
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

    static void RenderComponentMenuEmptyState(StringView title, StringView hint)
    {
        ImGui::Dummy(ImVec2(0.0f, 18.0f));
        ImGui::TextUnformatted(title.data(), title.data() + title.size());
        ImGui::PushStyleColor(ImGuiCol_Text, GetComponentMenuSecondaryTextColor());
        ImGui::TextWrapped("%.*s", static_cast<int>(hint.size()), hint.data());
        ImGui::PopStyleColor();
    }

    static void RenderSearchResults(const Ref<Scene>& scene, const Vector<Entity>& entities, const entt::registry& registry,
                                    const Map<String, Map<ComponentEditor::ComponentTypeID, ComponentEditor::ComponentInfo>>& componentInfos,
                                    ComponentMenuModel& menu, const String& query)
    {
        const ComponentMenuModel::SearchResults& results = menu.Search(query);
        const Vector<ComponentMenuModel::ComponentEntry>& components = menu.GetComponents();
        const Vector<ComponentMenuModel::ScriptEntry>& scripts = menu.GetScripts();
        const size_t matchCount = results.GetMatchCount();
        if (matchCount == 0)
        {
            RenderComponentMenuEmptyState("No matches", "Try another name or clear the search to browse categories.");
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, GetComponentMenuSecondaryTextColor());
            ImGui::Text("%zu %s", matchCount, matchCount == 1 ? "result" : "results");
            ImGui::PopStyleColor();

            if (!results.ComponentIndices.empty())
            {
                RenderMenuSectionLabel("Components");
                ImGui::PushID("ComponentSearchResults");
                for (size_t componentIndex : results.ComponentIndices)
                {
                    const ComponentMenuModel::ComponentEntry& entry = components[componentIndex];
                    const auto typeId = static_cast<ComponentEditor::ComponentTypeID>(entry.Id);
                    ImGui::PushID(typeId);
                    const size_t presence =
                      std::count_if(entities.begin(), entities.end(), [&](Entity entity) { return HasComponentByID(registry, entity, typeId); });
                    const bool alreadyAdded = presence == entities.size();
                    const StringView detail = presence == 0u ? StringView(entry.Group) : StringView("Missing from selection");
                    if (RenderComponentMenuItem(entry.Name, detail, alreadyAdded))
                    {
                        const ComponentEditor::ComponentInfo* info = FindComponentInfo(componentInfos, typeId);
                        if (info != nullptr)
                        {
                            AddComponentToEntities(scene, entities, registry, typeId, *info);
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
                    ImGui::PushID(scriptName.c_str());
                    const bool alreadyAdded =
                      std::all_of(entities.begin(), entities.end(), [&](Entity entity) { return EntityHasScript(entity, scriptEntry.Identity); });
                    if (RenderComponentMenuItem(scriptName, "C# script", alreadyAdded))
                    {
                        ComponentEditor::ComponentInfo dummy;
                        Ref<UndoActionGroup> actions = CreateRef<UndoActionGroup>(entities.size() == 1u ? "Add script" : "Add scripts");
                        for (Entity entity : entities)
                        {
                            if (!entity || EntityHasScript(entity, scriptEntry.Identity))
                                continue;
                            ChangeScriptComponentAction::State snapshot = ChangeScriptComponentAction::Capture(entity);
                            scene->AddScriptComponent(entity, scriptEntry.Identity, true);
                            actions->Add(CreateRef<ChangeScriptComponentAction>(entity, std::move(snapshot), "Add script"));
                        }
                        if (!actions->Empty())
                            UndoRedo::Get().RegisterAction(actions);
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::PopID();
                }
                ImGui::PopID();
            }
        }

        if (IsValidScriptClassName(query) && !results.ScriptNameDeclared)
        {
            ImGui::Dummy(ImVec2(0.0f, 6.0f));
            ImGui::Separator();
            RenderMenuSectionLabel("Create script");
            ImGui::PushID("CreateScript");
            const bool createRequested = RenderComponentMenuRow(results.CreateScriptLabel, "C# script", ComponentMenuRowAction::Add);
            ImGui::PopID();
            if (createRequested || (matchCount == 0 && Input::IsKeyPressed(Key::Enter)))
            {
                CreateNewScript(scene, entities, query);
                ImGui::CloseCurrentPopup();
            }
        }
    }

    static bool RenderComponentMenuBackRow(StringView category)
    {
        const float lineHeight = ImGui::GetTextLineHeight();
        const float rowHeight = lineHeight + 14.0f;
        ImGui::PushID("ComponentMenuBack");
        const bool clicked = ImGui::Selectable("##Back", false, ImGuiSelectableFlags_None, ImVec2(0.0f, rowHeight));
        const bool hovered = ImGui::IsItemHovered();
        const ImVec2 rowMin = ImGui::GetItemRectMin();
        const ImVec2 rowMax = ImGui::GetItemRectMax();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImU32 secondaryColor = ImGui::GetColorU32(GetComponentMenuSecondaryTextColor());
        const float textY = rowMin.y + (rowHeight - lineHeight) * 0.5f;
        const ImVec2 arrowCenter(rowMin.x + 12.0f, (rowMin.y + rowMax.y) * 0.5f);
        drawList->AddTriangleFilled(arrowCenter + ImVec2(3.0f, -5.0f), arrowCenter + ImVec2(3.0f, 5.0f),
                                    arrowCenter + ImVec2(-3.0f, 0.0f), secondaryColor);
        const float rootX = rowMin.x + 25.0f;
        drawList->AddText(ImVec2(rootX, textY), secondaryColor, "All components");
        const float separatorX = rootX + ImGui::CalcTextSize("All components").x + 8.0f;
        drawList->AddText(ImVec2(separatorX, textY), secondaryColor, "/");
        drawList->AddText(ImVec2(separatorX + 12.0f, textY), ImGui::GetColorU32(ImGuiCol_Text), category.data(),
                          category.data() + category.size());
        if (hovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImGui::PopID();
        return clicked;
    }

    static void RenderCategoryBrowser(const Ref<Scene>& scene, const Vector<Entity>& entities, const entt::registry& registry,
                                      const Map<String, Map<ComponentEditor::ComponentTypeID, ComponentEditor::ComponentInfo>>& componentInfos,
                                      ComponentMenuModel& menu, String& selectedCategory, bool& browsingScripts)
    {
        const Vector<ComponentMenuModel::ComponentEntry>& components = menu.GetComponents();
        const Vector<ComponentMenuModel::CategoryEntry>& categories = menu.GetCategories();
        const Vector<ComponentMenuModel::ScriptEntry>& scripts = menu.GetScripts();
        const size_t visibleScriptCount =
          std::count_if(scripts.begin(), scripts.end(), [](const ComponentMenuModel::ScriptEntry& entry) { return entry.Visible; });

        if (selectedCategory.empty() && !browsingScripts)
        {
            RenderMenuSectionLabel("Categories");
            ImGui::PushID("ComponentCategories");
            for (const ComponentMenuModel::CategoryEntry& category : categories)
            {
                String countLabel = std::to_string(category.ComponentCount);
                countLabel += category.ComponentCount == 1u ? " component" : " components";
                ImGui::PushID(category.Name.c_str());
                const bool selected = RenderComponentMenuRow(category.Name, countLabel, ComponentMenuRowAction::Navigate);
                ImGui::PopID();
                if (selected)
                {
                    selectedCategory = category.Name;
                    ImGui::PopID();
                    return;
                }
            }
            ImGui::PopID();

            if (visibleScriptCount != 0u)
            {
                String countLabel = std::to_string(visibleScriptCount);
                countLabel += visibleScriptCount == 1u ? " script" : " scripts";
                ImGui::PushID("ScriptsCategory");
                browsingScripts = RenderComponentMenuRow("Scripts", countLabel, ComponentMenuRowAction::Navigate);
                ImGui::PopID();
            }
            return;
        }

        const StringView categoryLabel = browsingScripts ? StringView("Scripts") : StringView(selectedCategory);
        if (RenderComponentMenuBackRow(categoryLabel))
        {
            selectedCategory.clear();
            browsingScripts = false;
            return;
        }
        ImGui::Separator();

        if (browsingScripts)
        {
            ImGui::PushID("ScriptBrowser");
            for (const ComponentMenuModel::ScriptEntry& script : scripts)
            {
                if (!script.Visible)
                    continue;
                const String& scriptName = script.Name;
                ImGui::PushID(scriptName.c_str());
                const bool alreadyAdded =
                  std::all_of(entities.begin(), entities.end(), [&](Entity entity) { return EntityHasScript(entity, script.Identity); });
                if (RenderComponentMenuItem(scriptName, {}, alreadyAdded))
                {
                    Ref<UndoActionGroup> actions = CreateRef<UndoActionGroup>(entities.size() == 1u ? "Add script" : "Add scripts");
                    for (Entity entity : entities)
                    {
                        if (!entity || EntityHasScript(entity, script.Identity))
                            continue;
                        ChangeScriptComponentAction::State snapshot = ChangeScriptComponentAction::Capture(entity);
                        scene->AddScriptComponent(entity, script.Identity, true);
                        actions->Add(CreateRef<ChangeScriptComponentAction>(entity, std::move(snapshot), "Add script"));
                    }
                    if (!actions->Empty())
                        UndoRedo::Get().RegisterAction(actions);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
            }
            ImGui::PopID();
            return;
        }

        const auto selected = std::find_if(categories.begin(), categories.end(),
                                           [&](const ComponentMenuModel::CategoryEntry& category) { return category.Name == selectedCategory; });
        if (selected == categories.end())
        {
            selectedCategory.clear();
            return;
        }

        ImGui::PushID("ComponentBrowser");
        const size_t lastComponentIndex = selected->FirstComponentIndex + selected->ComponentCount;
        for (size_t componentIndex = selected->FirstComponentIndex; componentIndex < lastComponentIndex; componentIndex++)
        {
            const ComponentMenuModel::ComponentEntry& entry = components[componentIndex];
            const auto typeId = static_cast<ComponentEditor::ComponentTypeID>(entry.Id);
            ImGui::PushID(typeId);
            const bool alreadyAdded =
              std::all_of(entities.begin(), entities.end(), [&](Entity entity) { return HasComponentByID(registry, entity, typeId); });
            if (RenderComponentMenuItem(entry.Name, {}, alreadyAdded))
            {
                const ComponentEditor::ComponentInfo* info = FindComponentInfo(componentInfos, typeId);
                if (info != nullptr)
                {
                    AddComponentToEntities(scene, entities, registry, typeId, *info);
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::PopID();
        }
        ImGui::PopID();
    }

    static void RenderAddComponentPopup(const Ref<Scene>& scene, Entity primary, const Vector<Entity>& entities, const entt::registry& registry,
                                        const Map<String, Map<ComponentEditor::ComponentTypeID, ComponentEditor::ComponentInfo>>& componentInfos,
                                        ComponentMenuModel& menu, String& query, String& selectedCategory, bool& browsingScripts,
                                        bool& grabSearchFocus)
    {
        ImGui::SetNextWindowSize(ImVec2(400.0f, 500.0f), ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints(ImVec2(340.0f, 400.0f), ImVec2(540.0f, 680.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));
        if (!ImGui::BeginPopup("Add Component"))
        {
            ImGui::PopStyleVar(2);
            return;
        }

        if (ImGui::IsWindowAppearing())
        {
            query.clear();
            selectedCategory.clear();
            browsingScripts = false;
            grabSearchFocus = true;
        }

        SynchronizeScriptCatalog(menu);

        ImGui::TextUnformatted("Add Component");
        ImGui::PushStyleColor(ImGuiCol_Text, GetComponentMenuSecondaryTextColor());
        if (entities.size() == 1u)
            ImGui::Text("Add to %s", primary.GetName().c_str());
        else
            ImGui::Text("Add to %zu selected entities", entities.size());
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 6.0f));
        UIUtils::SearchWidget(query, "Search components and scripts...", &grabSearchFocus);
        ImGui::PopStyleVar();
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        ImGui::Separator();

        ImGui::BeginChild("##AddComponentResults", ImVec2(0.0f, 0.0f), true);
        if (!query.empty())
            RenderSearchResults(scene, entities, registry, componentInfos, menu, query);
        else
            RenderCategoryBrowser(scene, entities, registry, componentInfos, menu, selectedCategory, browsingScripts);
        ImGui::EndChild();

        ImGui::EndPopup();
        ImGui::PopStyleVar(2);
    }

    void ComponentEditor::ResetUndoTransactions(bool finishInteraction)
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
                ResetUndoTransactions(false);
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
            ResetUndoTransactions(m_UndoScene == scene.get());
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

        RenderAddComponentPopup(scene, primary, m_SelectionScratch, registry, m_ComponentInfos, m_ComponentMenu, m_ComponentSearch,
                                m_ComponentBrowserCategory, m_ComponentBrowserScripts, m_GrabComponentSearchFocus);
        ImGui::PopID();
    }

} // namespace Crowny
