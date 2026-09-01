#include "cwepch.h"

#include "Editor/Editor.h"
#include "Editor/EditorAssets.h"
#include "Editor/EditorLayer.h"
#include "Editor/ViewportTransformInteraction.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Events/ImGuiEvent.h"
#include "Crowny/Input/Input.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/Scene/SceneRenderer.h"

#include "Panels/ViewportHudText.h"
#include "Panels/ViewportPanel.h"
#include "UI/UIUtils.h"

#include "Crowny/Ecs/Components.h"

#include "Crowny/ImGui/ImGuiVulkanTexture.h"
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>

namespace Crowny
{

    namespace
    {
        bool IsSupportedViewportAsset(const FileEntry* fileEntry)
        {
            if (fileEntry == nullptr || fileEntry->Metadata == nullptr)
                return false;

            const AssetType assetType = fileEntry->Metadata->Type;
            return assetType == AssetType::Scene || assetType == AssetType::Material || assetType == AssetType::Mesh ||
                   assetType == AssetType::AudioClip || assetType == AssetType::Prefab;
        }

        const char* GetViewportDropLabel(const FileEntry* fileEntry)
        {
            if (!IsSupportedViewportAsset(fileEntry))
                return "This asset cannot be used in the viewport";

            switch (fileEntry->Metadata->Type)
            {
            case AssetType::Scene:
                return "Drop to open scene";
            case AssetType::Material:
                return "Drop to apply material";
            case AssetType::Mesh:
                return "Drop to create mesh entity";
            case AssetType::AudioClip:
                return "Drop to create audio source";
            case AssetType::Prefab:
                return "Drop to instantiate prefab";
            default:
                return "This asset cannot be used in the viewport";
            }
        }

        const FileEntry* GetDraggedAsset()
        {
            const ImGuiPayload* payload = ImGui::GetDragDropPayload();
            if (payload == nullptr || !payload->IsDataType(ID_ASSET_ITEM_PAYLOAD) || payload->Data == nullptr)
                return nullptr;

            const LibraryEntry* entry = *static_cast<const LibraryEntry* const*>(payload->Data);
            if (entry == nullptr || entry->Type != LibraryEntryType::File)
                return nullptr;
            return static_cast<const FileEntry*>(entry);
        }

        void FormatStatisticCount(char* output, size_t outputSize, uint64_t value)
        {
            if (value >= 1'000'000'000ull)
                snprintf(output, outputSize, "%.2fB", static_cast<double>(value) / 1'000'000'000.0);
            else if (value >= 1'000'000ull)
                snprintf(output, outputSize, "%.2fM", static_cast<double>(value) / 1'000'000.0);
            else if (value >= 1'000ull)
                snprintf(output, outputSize, "%.1fK", static_cast<double>(value) / 1'000.0);
            else
                snprintf(output, outputSize, "%llu", static_cast<unsigned long long>(value));
        }

    } // namespace

    static ImGuizmo::OPERATION GetImGuizmoMode(GizmoEditMode gizmoMode)
    {
        switch (gizmoMode)
        {
        case GizmoEditMode::Translate:
            return ImGuizmo::TRANSLATE;
        case GizmoEditMode::Rotate:
            return ImGuizmo::ROTATE;
        case GizmoEditMode::Scale:
            return ImGuizmo::SCALE;
        case GizmoEditMode::Bounds:
            // ImGuizmo::BOUNDS requires explicit local bounds. The collider path
            // supplies those separately; generic selections use scale handles.
            return ImGuizmo::SCALE;
        }
        return ImGuizmo::TRANSLATE;
    }

    static SelectionTransformOperation GetSelectionTransformOperation(GizmoEditMode gizmoMode)
    {
        switch (gizmoMode)
        {
        case GizmoEditMode::Rotate:
            return SelectionTransformOperation::Rotate;
        case GizmoEditMode::Scale:
        case GizmoEditMode::Bounds:
            return SelectionTransformOperation::Scale;
        case GizmoEditMode::None:
        case GizmoEditMode::Translate:
            return SelectionTransformOperation::Translate;
        }
        return SelectionTransformOperation::Translate;
    }

    ViewportPanel::ViewportPanel(const String& name, std::function<Entity()> selectedEntity, std::function<const Vector<Entity>&()> selectedEntities)
      : ImGuiPanel(name), m_ViewportBounds(0.0f), m_SelectedEntity(std::move(selectedEntity)), m_SelectedEntities(std::move(selectedEntities))
    {
        Ref<ProjectSettings> projSettings = Editor::Get().GetProjectSettings();
        m_GizmoMode = projSettings->GizmoMode;
        m_LocalMode = projSettings->GizmoLocalMode;
    }

    ViewportPanel::~ViewportPanel() { CancelActiveInteractions(); }

    const Vector<Entity>& ViewportPanel::RefreshSelectionScratch(Entity primary)
    {
        m_SelectedEntitiesScratch.clear();
        if (m_SelectedEntities)
        {
            const Vector<Entity>& selectedEntities = m_SelectedEntities();
            m_SelectedEntitiesScratch.reserve(selectedEntities.size() + (primary ? 1u : 0u));
            for (Entity entity : selectedEntities)
            {
                if (entity.IsValid())
                    m_SelectedEntitiesScratch.push_back(entity);
            }
        }

        if (primary && std::find(m_SelectedEntitiesScratch.begin(), m_SelectedEntitiesScratch.end(), primary) == m_SelectedEntitiesScratch.end())
            m_SelectedEntitiesScratch.push_back(primary);

        return m_SelectedEntitiesScratch;
    }

    void ViewportPanel::EndTransformInteraction()
    {
        const ViewportTransformResolution resolution = m_TransformInteraction.Resolve(false, false);
        UndoRedo::Get().RegisterAction(resolution.Action);
    }

    void ViewportPanel::CancelTransformInteraction() { m_TransformInteraction.Cancel(); }

    void ViewportPanel::EndColliderBoundsInteraction(bool cancel)
    {
        if (cancel)
            m_ColliderBoundsTransaction.Cancel();
        else
            UndoRedo::Get().RegisterAction(m_ColliderBoundsTransaction.Commit());
    }

    void ViewportPanel::CancelActiveInteractions()
    {
        if (m_TransformInteraction.IsActive())
            CancelTransformInteraction();
        m_GizmoWasUsing = false;
        m_ColliderBoundsTransaction.Cancel();
    }

    void ViewportPanel::DrawViewportHud(const ImVec2& imageMin, const ImVec2& imageMax, Entity selectedEntity, const Vector<Entity>& selectedEntities)
    {
        const float viewportWidth = imageMax.x - imageMin.x;
        const float viewportHeight = imageMax.y - imageMin.y;
        if (viewportWidth < 154.0f || viewportHeight < 58.0f)
            return;

        const bool compactStrip = viewportWidth < 170.0f;
        const float padding = 4.0f;
        const float spacing = 2.0f;
        const float settingsGap = compactStrip ? 3.0f : 5.0f;
        const float buttonSize = compactStrip ? 20.0f : 22.0f;
        constexpr size_t ButtonCount = 6u;
        const float hudWidth = padding * 2.0f + buttonSize * static_cast<float>(ButtonCount) + spacing * 4.0f + settingsGap * 2.0f;
        const ImVec2 hudMin(imageMin.x + 6.0f, imageMin.y + 5.0f);
        const ImVec2 hudMax(hudMin.x + hudWidth, hudMin.y + buttonSize + padding * 2.0f);
        m_MouseOverHud |= ImGui::IsMouseHoveringRect(hudMin, hudMax, false);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(hudMin, hudMax, IM_COL32(27, 24, 22, 220), 4.0f);
        drawList->AddRect(hudMin, hudMax, IM_COL32(90, 81, 73, 180), 4.0f);

        const ImVec2 savedCursor = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(hudMin.x + padding, hudMin.y + padding));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, 0.0f));
        ImGui::PushID("ViewportToolStrip");

        const ImU32 neutralTint = IM_COL32(166, 158, 150, 255);
        const ImU32 hoverTint = IM_COL32(235, 232, 228, 255);
        const auto drawIconButton = [&](const char* id, const Ref<Texture>& icon, bool active, StringView tooltip, bool settings = false) {
            ImGui::PushID(id);
            const bool clicked = ImGui::InvisibleButton("##icon", ImVec2(buttonSize, buttonSize));
            const ImRect bounds = UI::GetItemRect();
            if (active || settings)
            {
                const ImU32 fill = active ? IM_COL32(196, 123, 48, 42) : IM_COL32(255, 255, 255, 12);
                drawList->AddRectFilled(bounds.Min, bounds.Max, fill, settings ? 4.0f : 3.0f);
            }
            if (settings)
                drawList->AddRect(bounds.Min, bounds.Max, active ? UI::Colors::Accent : IM_COL32(104, 94, 85, 190), 4.0f);

            const float iconPadding = 2.5f;
            const ImRect iconBounds(ImVec2(bounds.Min.x + iconPadding, bounds.Min.y + iconPadding),
                                    ImVec2(bounds.Max.x - iconPadding, bounds.Max.y - iconPadding));
            UI::DrawButtonImage(icon, active ? UI::Colors::Accent : neutralTint, active ? UI::Colors::AccentHover : hoverTint,
                                UI::Colors::AccentPress, iconBounds);
            UI::SetTooltip(tooltip);
            ImGui::PopID();
            return clicked;
        };

        const EditorAssetsLibrary assets = EditorAssets::Get();
        if (drawIconButton("Select", assets.ArrowPointerIcon, m_GizmoMode == GizmoEditMode::None, "Select (Q)"))
            m_GizmoMode = GizmoEditMode::None;
        ImGui::SameLine();
        if (drawIconButton("Move", assets.ArrowsIcon, m_GizmoMode == GizmoEditMode::Translate, "Move (W)"))
            m_GizmoMode = GizmoEditMode::Translate;
        ImGui::SameLine();
        if (drawIconButton("Rotate", assets.RotateIcon, m_GizmoMode == GizmoEditMode::Rotate, "Rotate (E)"))
            m_GizmoMode = GizmoEditMode::Rotate;
        ImGui::SameLine();
        if (drawIconButton("Scale", assets.MaximizeIcon, m_GizmoMode == GizmoEditMode::Scale || m_GizmoMode == GizmoEditMode::Bounds,
                           "Scale (R). Bounds editing: T"))
            m_GizmoMode = GizmoEditMode::Scale;
        ImGui::SameLine();
        if (drawIconButton("Space", assets.GlobeIcon, m_LocalMode, m_LocalMode ? "Local transform space (X)" : "World transform space (X)"))
            m_LocalMode = !m_LocalMode;

        ImGui::SameLine(0.0f, settingsGap);
        const ImVec2 separatorTop = ImGui::GetCursorScreenPos();
        drawList->AddLine(ImVec2(separatorTop.x - settingsGap * 0.5f, separatorTop.y + 3.0f),
                          ImVec2(separatorTop.x - settingsGap * 0.5f, separatorTop.y + buttonSize - 3.0f), IM_COL32(104, 94, 85, 150));
        const bool settingsOpen = m_IsViewportSettingsOpen && m_IsViewportSettingsOpen();
        if (drawIconButton("Settings", assets.SettingsIcon, settingsOpen, "Viewport, grid, snapping, and physics gizmo settings", true) &&
            m_ToggleViewportSettings)
            m_ToggleViewportSettings();

        ImGui::PopID();
        ImGui::PopStyleVar();
        ImGui::SetCursorScreenPos(savedCursor);
        ImGui::Dummy(ImVec2(0.0f, 0.0f));

        if (viewportWidth >= 670.0f)
        {
            const EditorCamera& camera = EditorLayer::GetEditorCamera();
            const StringView entityName = selectedEntity ? StringView(selectedEntity.GetName()) : StringView{};
            const ViewportHudStatus status =
              FormatViewportHudStatus(entityName, static_cast<bool>(selectedEntity), selectedEntities.size(), static_cast<int32_t>(m_ViewportSize.x),
                                      static_cast<int32_t>(m_ViewportSize.y), camera.GetDistance());
            const ImVec2 textSize = ImGui::CalcTextSize(status.Text.data());
            const ImVec2 statusMin(imageMax.x - textSize.x - 16.0f, imageMax.y - ImGui::GetFrameHeight() - 12.0f);
            const ImVec2 statusMax(imageMax.x - 8.0f, imageMax.y - 8.0f);
            if (statusMin.x > imageMin.x + 8.0f)
            {
                ImGui::GetWindowDrawList()->AddRectFilled(statusMin, statusMax, IM_COL32(27, 24, 22, 210), 5.0f);
                ImGui::GetWindowDrawList()->AddText(ImVec2(statusMin.x + 4.0f, statusMin.y + ImGui::GetStyle().FramePadding.y),
                                                    IM_COL32(190, 184, 178, 255), status.Text.data());
            }
        }

        if (m_ShowStatistics && viewportWidth >= 260.0f && viewportHeight >= 120.0f && RenderAPI::TryGet() != nullptr)
        {
            const RenderFrameStatistics frame = RenderAPI::TryGet()->GetFrameStatistics();
            const SceneRenderStatistics scene = SceneRenderer::GetStatistics();
            const uint64_t vertices = scene.FrameNumber != 0 ? scene.VisibleVertices : frame.Vertices;
            const uint64_t triangles = scene.FrameNumber != 0 ? scene.VisibleTriangles : frame.Triangles;

            char vertexCount[20];
            char triangleCount[20];
            FormatStatisticCount(vertexCount, sizeof(vertexCount), vertices);
            FormatStatisticCount(triangleCount, sizeof(triangleCount), triangles);

            char lines[4][112];
            snprintf(lines[0], sizeof(lines[0]), "%.0f FPS   %.2f ms", frame.FramesPerSecond, frame.FrameTimeMs);
            snprintf(lines[1], sizeof(lines[1]), "Draws %llu   Verts %s   Tris %s", static_cast<unsigned long long>(frame.DrawCalls), vertexCount,
                     triangleCount);
            snprintf(lines[2], sizeof(lines[2]), "Visible %u / %u   Lights %u", scene.VisibleInstances, scene.ActiveInstances, scene.ActiveLights);
            snprintf(lines[3], sizeof(lines[3]), "Passes %u   RG CPU %.2f ms", scene.RenderPasses, scene.RenderGraphCpuTimeMs);

            const uint32_t lineCount = viewportWidth >= 430.0f ? 4u : 3u;
            float textWidth = 0.0f;
            for (uint32_t line = 0; line < lineCount; line++)
                textWidth = std::max(textWidth, ImGui::CalcTextSize(lines[line]).x);

            const float lineHeight = ImGui::GetTextLineHeight();
            const float viewCubeSize = std::clamp(std::min(viewportWidth, viewportHeight) * 0.22f, 72.0f, 128.0f);
            const ImVec2 statisticsMin(imageMax.x - textWidth - 24.0f, imageMin.y + viewCubeSize + 12.0f);
            const ImVec2 statisticsMax(imageMax.x - 8.0f, statisticsMin.y + lineHeight * lineCount + 16.0f);
            if (statisticsMax.y > imageMax.y - 8.0f)
                return;
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(statisticsMin, statisticsMax, IM_COL32(27, 24, 22, 224), 5.0f);
            drawList->AddRect(statisticsMin, statisticsMax, IM_COL32(76, 68, 61, 210), 5.0f);
            for (uint32_t line = 0; line < lineCount; line++)
            {
                const ImU32 color = line == 0 ? IM_COL32(235, 232, 228, 255) : IM_COL32(190, 184, 178, 255);
                drawList->AddText(ImVec2(statisticsMin.x + 8.0f, statisticsMin.y + 8.0f + lineHeight * line), color, lines[line]);
            }

            const bool statisticsHovered = ImGui::IsMouseHoveringRect(statisticsMin, statisticsMax, false);
            m_MouseOverHud |= statisticsHovered;
            if (statisticsHovered)
            {
                ImGui::BeginTooltip();
                ImGui::Text("Direct draws: %llu", static_cast<unsigned long long>(frame.DirectDrawCalls));
                ImGui::Text("Indirect batches: %llu", static_cast<unsigned long long>(frame.IndirectDrawCalls));
                ImGui::Text("Known indirect commands: %llu", static_cast<unsigned long long>(frame.IndirectCommands));
                ImGui::Text("Compute dispatches: %llu", static_cast<unsigned long long>(frame.ComputeDispatches));
                ImGui::Separator();
                ImGui::Text("Main-thread allocations: %llu (%.2f KiB)", static_cast<unsigned long long>(frame.MainThreadAllocations),
                            static_cast<double>(frame.MainThreadAllocatedBytes) / 1024.0);
                ImGui::Text("Graph: %u graphics, %u compute, %u transfer", scene.GraphicsPasses, scene.ComputePasses, scene.TransferPasses);
                ImGui::Text("Scheduled barriers: %u", scene.Barriers);
                ImGui::Text("Scene upload: %.2f KiB", static_cast<double>(scene.UploadedBytes) / 1024.0);
                ImGui::EndTooltip();
            }
        }
    }

    void ViewportPanel::Render()
    {
        m_MouseOverHud = false;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
        if (!BeginPanel())
        {
            CancelActiveInteractions();
            EndPanel();
            ImGui::PopStyleVar();
            return;
        }
        Application::TryGet()->GetImGuiLayer()->BlockEvents(!m_Hovered);

        if (m_Hovered && GImGui->ActiveId == 0)
        {
            if (!Input::IsMouseButtonPressed(Mouse::ButtonRight)) // && m_CurrentScene != m_RuntimeScene)
            {
                if (Input::IsKeyPressed(Key::Q))
                    m_GizmoMode = GizmoEditMode::None;
                if (Input::IsKeyPressed(Key::W))
                    m_GizmoMode = GizmoEditMode::Translate;
                if (Input::IsKeyPressed(Key::E))
                    m_GizmoMode = GizmoEditMode::Rotate;
                if (Input::IsKeyPressed(Key::R))
                    m_GizmoMode = GizmoEditMode::Scale;
                if (Input::IsKeyPressed(Key::T))
                    m_GizmoMode = GizmoEditMode::Bounds;
                if (Input::IsKeyDown(Key::X))
                    m_LocalMode = !m_LocalMode;
                if (Input::IsKeyDown(Key::F))
                {
                    const Entity selectedEntity = m_SelectedEntity ? m_SelectedEntity() : Entity{};
                    const Vector<Entity>& selectedEntities = RefreshSelectionScratch(selectedEntity);
                    if (!selectedEntities.empty())
                    {
                        const glm::mat4 pivot = ViewportTransformInteraction::CalculatePivot(
                          selectedEntities, selectedEntity, m_LocalMode ? SelectionTransformSpace::Local : SelectionTransformSpace::World);
                        EditorLayer::GetEditorCamera().Focus(glm::vec3(pivot[3]));
                    }
                }
            }
        }

        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
        viewportPanelSize.x = std::max(viewportPanelSize.x, 1.0f);
        viewportPanelSize.y = std::max(viewportPanelSize.y, 1.0f);

        m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };
        const Ref<Texture> texture = m_RenderTarget ? m_RenderTarget->GetColorTexture(0) : nullptr;
        if (texture)
        {
            const ImTextureID textureID = ImGuiVulkanTexture::Get(texture);
            ImGui::Image(textureID, ImVec2(m_ViewportSize.x, m_ViewportSize.y), ImVec2{ 0, 1 }, ImVec2{ 1, 0 }); // The viewport itself
        }
        else
        {
            CancelActiveInteractions();
            ImGui::Dummy(ImVec2(m_ViewportSize.x, m_ViewportSize.y));
        }

        const ImVec2 imageMin = ImGui::GetItemRectMin();
        const ImVec2 imageMax = ImGui::GetItemRectMax();
        m_ViewportBounds = { imageMin.x, imageMin.y, imageMax.x, imageMax.y };

        if (texture && ImGui::BeginDragDropTarget())
        {
            const FileEntry* draggedAsset = GetDraggedAsset();
            const bool validAsset = IsSupportedViewportAsset(draggedAsset);
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(imageMin, imageMax, validAsset ? IM_COL32(32, 102, 66, 58) : IM_COL32(135, 42, 42, 58));
            drawList->AddRect(imageMin, imageMax, validAsset ? IM_COL32(83, 190, 123, 255) : IM_COL32(220, 84, 84, 255), 0.0f, 0, 3.0f);

            const char* dropLabel = GetViewportDropLabel(draggedAsset);
            if (ImGui::CalcTextSize(dropLabel).x > imageMax.x - imageMin.x - 24.0f)
                dropLabel = validAsset ? "Drop asset" : "Unsupported asset";
            const ImVec2 labelSize = ImGui::CalcTextSize(dropLabel);
            const ImVec2 labelMin((imageMin.x + imageMax.x - labelSize.x) * 0.5f - 12.0f, (imageMin.y + imageMax.y - labelSize.y) * 0.5f - 8.0f);
            const ImVec2 labelMax(labelMin.x + labelSize.x + 24.0f, labelMin.y + labelSize.y + 16.0f);
            drawList->AddRectFilled(labelMin, labelMax, IM_COL32(24, 22, 20, 235), 5.0f);
            drawList->AddText(ImVec2(labelMin.x + 12.0f, labelMin.y + 8.0f), IM_COL32(235, 232, 228, 255), dropLabel);

            if (const FileEntry* fileEntry = UIUtils::AcceptAssetPayload(IsSupportedViewportAsset))
            {
                const ImVec2 mousePosition = ImGui::GetMousePos();
                ImGuiViewportSceneDraggedEvent fileDragEvent(fileEntry, glm::vec2(mousePosition.x, mousePosition.y));
                if (OnEvent)
                    OnEvent(fileDragEvent);
            }
            ImGui::EndDragDropTarget();
        }

        Entity selected = m_SelectedEntity ? m_SelectedEntity() : Entity{};
        const Vector<Entity>& selectedEntities = RefreshSelectionScratch(selected);

        DrawViewportHud(imageMin, imageMax, selected, selectedEntities);

        EditorCamera& camera = EditorLayer::GetEditorCamera();
        const glm::mat4& proj = camera.GetProjection();
        glm::mat4 view = camera.GetViewMatrix();
        ImGuizmo::SetRect(imageMin.x, imageMin.y, imageMax.x - imageMin.x, imageMax.y - imageMin.y);
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();

        if (selected && m_GizmoMode != GizmoEditMode::None)
        {
            const Ref<EditorSettings> editorSettings = Editor::Get().GetEditorSettings();
            const bool snap = m_SnapEnabled || Input::IsKeyPressed(Key::LeftControl) || Input::IsKeyPressed(Key::RightControl);
            float snapValues[3];
            if (m_GizmoMode == GizmoEditMode::Rotate)
            {
                const float value = std::max(editorSettings->GridRotateSnap, 0.1f);
                snapValues[0] = snapValues[1] = snapValues[2] = value;
            }
            else if (m_GizmoMode == GizmoEditMode::Scale || m_GizmoMode == GizmoEditMode::Bounds)
            {
                const float value = std::max(editorSettings->GridScaleSnap, 0.001f);
                snapValues[0] = snapValues[1] = snapValues[2] = value;
            }
            else
            {
                snapValues[0] = std::max(editorSettings->GridMoveSnap.x, 0.001f);
                snapValues[1] = std::max(editorSettings->GridMoveSnap.y, 0.001f);
                snapValues[2] = std::max(editorSettings->GridMoveSnap.z, 0.001f);
            }
            ImGuizmo::AllowAxisFlip(false);

            if (m_GizmoMode == GizmoEditMode::Bounds && selectedEntities.size() == 1u && selected.HasComponent<BoxCollider2DComponent>())
            {
                if (m_GizmoWasUsing)
                {
                    EndTransformInteraction();
                    m_GizmoWasUsing = false;
                }
                if (m_ColliderBoundsTransaction.IsActive() && !m_ColliderBoundsTransaction.Owns(selected))
                    EndColliderBoundsInteraction(Input::IsKeyPressed(Key::Escape));

                auto& bc2d = selected.GetComponent<BoxCollider2DComponent>();
                glm::mat4 entityWorld = selected.GetWorldMatrix();
                glm::mat4 colliderTransform = entityWorld * glm::translate(glm::mat4(1.0f), glm::vec3(bc2d.GetOffset(), 0.0f)) *
                                              glm::scale(glm::mat4(1.0f), glm::vec3(bc2d.GetSize() * 2.0f, 1.0f));

                float localBounds[6] = { -0.5f, -0.5f, 0.0f, 0.5f, 0.5f, 0.0f };

                if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), ImGuizmo::BOUNDS, ImGuizmo::LOCAL,
                                         glm::value_ptr(colliderTransform), nullptr, snap ? snapValues : nullptr, localBounds))
                {
                    glm::vec3 newWorldCenter, newWorldScale;
                    glm::quat newRot;
                    Math::DecomposeMatrix(colliderTransform, newWorldCenter, newRot, newWorldScale);

                    glm::vec3 localCenter = glm::inverse(entityWorld) * glm::vec4(newWorldCenter, 1.0f);
                    glm::vec2 newOffset = { localCenter.x, localCenter.y };

                    glm::vec3 entityPos, entityScale;
                    glm::quat entityRot;
                    Math::DecomposeMatrix(entityWorld, entityPos, entityRot, entityScale);
                    glm::vec2 newSize = glm::vec2(newWorldScale.x, newWorldScale.y) / glm::vec2(entityScale.x, entityScale.y) / 2.0f;

                    if (newSize.x > 0.0f && newSize.y > 0.0f)
                    {
                        if (!m_ColliderBoundsTransaction.IsActive())
                            m_ColliderBoundsTransaction.Begin(selected);
                        m_ColliderBoundsTransaction.Update(newOffset, newSize);
                    }
                }

                if (m_ColliderBoundsTransaction.IsActive())
                {
                    if (Input::IsKeyPressed(Key::Escape))
                        EndColliderBoundsInteraction(true);
                    else if (!ImGuizmo::IsUsing())
                        EndColliderBoundsInteraction(false);
                }
            }
            else
            {
                if (m_ColliderBoundsTransaction.IsActive())
                    EndColliderBoundsInteraction(Input::IsKeyPressed(Key::Escape));

                const SelectionTransformOperation selectionOperation = GetSelectionTransformOperation(m_GizmoMode);
                const SelectionTransformSpace selectionSpace =
                  m_LocalMode || m_GizmoMode == GizmoEditMode::Bounds ? SelectionTransformSpace::Local : SelectionTransformSpace::World;
                glm::mat4 transform = m_TransformInteraction.IsActive()
                                        ? m_TransformInteraction.GetCurrentPivot()
                                        : ViewportTransformInteraction::CalculatePivot(selectedEntities, selected, selectionSpace);
                const ImGuizmo::OPERATION operation = GetImGuizmoMode(m_GizmoMode);
                const bool manipulated = ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), operation, ImGuizmo::LOCAL,
                                                              glm::value_ptr(transform), nullptr, snap ? snapValues : nullptr);
                const bool usingGizmo = ImGuizmo::IsUsing();
                const ViewportTransformFrameResult frame = m_TransformInteraction.ProcessGizmoFrame(
                  selectedEntities, selected, selectionOperation, selectionSpace,
                  ViewportGizmoFrame{ transform, manipulated, usingGizmo, Input::IsKeyPressed(Key::Escape) });
                UndoRedo::Get().RegisterAction(frame.Resolution.Action);
                m_GizmoWasUsing = usingGizmo;
            }
        }
        else if (m_GizmoWasUsing)
        {
            EndTransformInteraction();
            m_GizmoWasUsing = false;
        }
        else if (m_ColliderBoundsTransaction.IsActive())
            EndColliderBoundsInteraction(Input::IsKeyPressed(Key::Escape));
        const float viewportMinSize = std::min(m_ViewportSize.x, m_ViewportSize.y);
        const float viewCubeSize = std::clamp(viewportMinSize * 0.22f, 72.0f, 128.0f);
        if (viewportMinSize >= 100.0f &&
            ImGuizmo::ViewManipulate(glm::value_ptr(view), camera.GetDistance(), { imageMax.x - viewCubeSize - 8.0f, imageMin.y + 4.0f },
                                     ImVec2(viewCubeSize, viewCubeSize), 0x10101010))
        {
            // ViewManipulate modifies the view matrix in-place. Invert it to get the
            // camera transform (world-space position + orientation) before decomposing.
            glm::mat4 cameraTransform = glm::inverse(view);
            glm::vec3 t, s;
            glm::quat orientation;
            Math::DecomposeMatrix(cameraTransform, t, orientation, s);
            // The camera builds its orientation as quat(vec3(-pitch, -yaw, roll)),
            // so recover pitch/yaw/roll from the extracted Euler angles.
            glm::vec3 euler = glm::eulerAngles(orientation);
            camera.SetPitch(-euler.x);
            camera.SetYaw(-euler.y);
            camera.SetRoll(euler.z);
        }

        EndPanel();
        ImGui::PopStyleVar();
    }

    void ViewportPanel::SetEventCallback(const EventCallbackFn& onEvent) { OnEvent = onEvent; }

    void ViewportPanel::SetEditorRenderTarget(const Ref<RenderTexture>& rt) { m_RenderTarget = rt; }

} // namespace Crowny
