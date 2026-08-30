#include "cwepch.h"

#include "Editor/Editor.h"
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
        constexpr UndoTransaction::Id TransformTransactionId = 1u;

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

        const char* GetGizmoLabel(GizmoEditMode mode)
        {
            switch (mode)
            {
            case GizmoEditMode::None:
                return "Select";
            case GizmoEditMode::Translate:
                return "Move";
            case GizmoEditMode::Rotate:
                return "Rotate";
            case GizmoEditMode::Scale:
                return "Scale";
            case GizmoEditMode::Bounds:
                return "Bounds";
            default:
                return "Select";
            }
        }

        bool HudButton(const char* label, bool active, const ImVec2& size)
        {
            if (active)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, UI::Colors::Accent);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UI::Colors::AccentHover);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, UI::Colors::AccentPress);
            }

            const bool pressed = ImGui::Button(label, size);
            if (active)
                ImGui::PopStyleColor(3);
            return pressed;
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

        class WorldTransformAction : public UndoAction
        {
        public:
            WorldTransformAction(Entity target, const glm::mat4& oldTransform, const glm::mat4& newTransform)
              : UndoAction("Transform entity"), m_Scene(target.GetScene()), m_Target(target.GetUuid()), m_OldTransform(oldTransform),
                m_NewTransform(newTransform)
            {
            }

            void Commit() override
            {
                if (Entity target = Resolve())
                    target.SetWorldTransform(m_NewTransform);
            }
            void Revert() override
            {
                if (Entity target = Resolve())
                    target.SetWorldTransform(m_OldTransform);
            }

        private:
            Entity Resolve() const { return m_Scene ? m_Scene->TryGetEntityFromUuid(m_Target) : Entity{}; }

            Ref<Scene> m_Scene;
            UUID m_Target = UUID::EMPTY;
            glm::mat4 m_OldTransform{ 1.0f };
            glm::mat4 m_NewTransform{ 1.0f };
        };

        bool MatrixChanged(const glm::mat4& lhs, const glm::mat4& rhs)
        {
            for (uint32_t column = 0; column < 4u; ++column)
            {
                if (glm::any(glm::greaterThan(glm::abs(lhs[column] - rhs[column]), glm::vec4(0.00001f))))
                    return true;
            }
            return false;
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
            return ImGuizmo::BOUNDS;
        }
        return ImGuizmo::TRANSLATE;
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

    const Vector<Entity>& ViewportPanel::GetTopLevelSelection(const Vector<Entity>& selectedEntities)
    {
        m_TopLevelSelectionScratch.clear();
        m_TopLevelSelectionScratch.reserve(selectedEntities.size());
        for (Entity entity : selectedEntities)
        {
            if (!entity)
                continue;
            bool selectedAncestor = false;
            for (Entity parent = entity.GetParent(); parent; parent = parent.GetParent())
            {
                if (std::find(selectedEntities.begin(), selectedEntities.end(), parent) != selectedEntities.end())
                {
                    selectedAncestor = true;
                    break;
                }
            }
            if (!selectedAncestor)
                m_TopLevelSelectionScratch.push_back(entity);
        }
        return m_TopLevelSelectionScratch;
    }

    glm::mat4 ViewportPanel::GetSelectionPivot(Entity primary, const Vector<Entity>& selectedEntities) const
    {
        glm::vec3 minimum(FLT_MAX);
        glm::vec3 maximum(-FLT_MAX);
        for (Entity entity : selectedEntities)
        {
            if (!entity)
                continue;
            const glm::vec3 position = entity.GetWorldPosition();
            minimum = glm::min(minimum, position);
            maximum = glm::max(maximum, position);
        }
        const glm::vec3 center = minimum.x == FLT_MAX ? glm::vec3(0.0f) : (minimum + maximum) * 0.5f;
        glm::mat4 pivot = glm::translate(glm::mat4(1.0f), center);
        if (m_LocalMode && primary)
            pivot *= glm::mat4_cast(primary.GetWorldRotation());
        return pivot;
    }

    void ViewportPanel::BeginTransformInteraction(const Vector<Entity>& selectedEntities, const glm::mat4& pivot)
    {
        if (m_TransformTransaction.IsActive())
            EndTransformInteraction();

        m_TransformSnapshots.clear();
        const Vector<Entity>& topLevelSelection = GetTopLevelSelection(selectedEntities);
        m_TransformSnapshots.reserve(topLevelSelection.size());
        for (Entity entity : topLevelSelection)
            m_TransformSnapshots.push_back({ Ref<Scene>(entity.GetScene()), entity, entity.GetWorldMatrix() });
        m_InitialGizmoTransform = pivot;
        m_CurrentGizmoTransform = pivot;
        m_TransformTransaction.Begin(TransformTransactionId, [this] { return BuildTransformAction(); });
    }

    void ViewportPanel::ApplyTransformInteraction(const glm::mat4& pivot)
    {
        m_CurrentGizmoTransform = pivot;
        const glm::mat4 delta = pivot * glm::inverse(m_InitialGizmoTransform);
        for (TransformSnapshot& snapshot : m_TransformSnapshots)
        {
            if (snapshot.Target)
                snapshot.Target.SetWorldTransform(delta * snapshot.WorldTransform);
        }
        m_TransformTransaction.Update(TransformTransactionId, MatrixChanged(m_InitialGizmoTransform, m_CurrentGizmoTransform));
    }

    void ViewportPanel::EndTransformInteraction()
    {
        UndoRedo::Get().RegisterAction(m_TransformTransaction.Commit(TransformTransactionId));
        m_TransformSnapshots.clear();
    }

    void ViewportPanel::CancelTransformInteraction()
    {
        for (TransformSnapshot& snapshot : m_TransformSnapshots)
        {
            if (snapshot.Target)
                snapshot.Target.SetWorldTransform(snapshot.WorldTransform);
        }
        m_TransformTransaction.Cancel(TransformTransactionId);
        m_TransformSnapshots.clear();
    }

    void ViewportPanel::EndColliderBoundsInteraction(bool cancel)
    {
        if (cancel)
            m_ColliderBoundsTransaction.Cancel();
        else
            UndoRedo::Get().RegisterAction(m_ColliderBoundsTransaction.Commit());
    }

    void ViewportPanel::CancelActiveInteractions()
    {
        if (m_TransformTransaction.IsActive())
            CancelTransformInteraction();
        else
            m_TransformSnapshots.clear();
        m_GizmoWasUsing = false;
        m_ColliderBoundsTransaction.Cancel();
    }

    Ref<UndoAction> ViewportPanel::BuildTransformAction() const
    {
        Ref<UndoActionGroup> actions = CreateRef<UndoActionGroup>(m_TransformSnapshots.size() == 1u ? "Transform entity" : "Transform entities");
        for (const TransformSnapshot& snapshot : m_TransformSnapshots)
        {
            if (!snapshot.Target)
                continue;
            const glm::mat4 current = snapshot.Target.GetWorldMatrix();
            if (MatrixChanged(snapshot.WorldTransform, current))
                actions->Add(CreateRef<WorldTransformAction>(snapshot.Target, snapshot.WorldTransform, current));
        }
        if (actions->Empty())
            return {};
        return actions;
    }

    void ViewportPanel::DrawViewportHud(const ImVec2& imageMin, const ImVec2& imageMax, Entity selectedEntity, const Vector<Entity>& selectedEntities)
    {
        const float viewportWidth = imageMax.x - imageMin.x;
        const float viewportHeight = imageMax.y - imageMin.y;
        if (viewportWidth < 155.0f || viewportHeight < 72.0f)
            return;

        const bool compact = viewportWidth < 430.0f;
        const bool showSpace = viewportWidth >= 245.0f;
        const bool showSnapValue = viewportWidth >= 355.0f;
        const bool showHelp = viewportWidth >= 205.0f;
        const float padding = 4.0f;
        const float spacing = 4.0f;
        const float buttonHeight = ImGui::GetFrameHeight();
        const float modeWidth = compact ? 64.0f : 78.0f;
        const float spaceWidth = compact ? 51.0f : 58.0f;
        const float snapWidth = 50.0f;
        const float valueWidth = 67.0f;
        const float helpWidth = 26.0f;

        float hudWidth = padding * 2.0f + modeWidth + spacing + snapWidth;
        if (showSpace)
            hudWidth += spacing + spaceWidth;
        if (showSnapValue)
            hudWidth += spacing + valueWidth;
        if (showHelp)
            hudWidth += spacing + helpWidth;

        const ImVec2 hudMin(imageMin.x + 8.0f, imageMax.y - buttonHeight - padding * 2.0f - 8.0f);
        const ImVec2 hudMax(hudMin.x + hudWidth, hudMin.y + buttonHeight + padding * 2.0f);
        m_MouseOverHud = ImGui::IsMouseHoveringRect(hudMin, hudMax, false);
        ImGui::GetWindowDrawList()->AddRectFilled(hudMin, hudMax, IM_COL32(27, 24, 22, 224), 5.0f);
        ImGui::GetWindowDrawList()->AddRect(hudMin, hudMax, IM_COL32(76, 68, 61, 210), 5.0f);

        const ImVec2 savedCursor = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(hudMin.x + padding, hudMin.y + padding));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 3.0f));

        if (HudButton(GetGizmoLabel(m_GizmoMode), m_GizmoMode != GizmoEditMode::None, ImVec2(modeWidth, buttonHeight)))
            ImGui::OpenPopup("##ViewportGizmoMenu");
        UI::SetTooltip("Choose a gizmo. Shortcuts: Q, W, E, R, T");

        if (ImGui::BeginPopup("##ViewportGizmoMenu"))
        {
            m_MouseOverHud |= ImGui::IsWindowHovered();
            if (ImGui::MenuItem("Select", "Q", m_GizmoMode == GizmoEditMode::None))
                m_GizmoMode = GizmoEditMode::None;
            if (ImGui::MenuItem("Move", "W", m_GizmoMode == GizmoEditMode::Translate))
                m_GizmoMode = GizmoEditMode::Translate;
            if (ImGui::MenuItem("Rotate", "E", m_GizmoMode == GizmoEditMode::Rotate))
                m_GizmoMode = GizmoEditMode::Rotate;
            if (ImGui::MenuItem("Scale", "R", m_GizmoMode == GizmoEditMode::Scale))
                m_GizmoMode = GizmoEditMode::Scale;
            if (ImGui::MenuItem("Bounds", "T", m_GizmoMode == GizmoEditMode::Bounds))
                m_GizmoMode = GizmoEditMode::Bounds;
            ImGui::EndPopup();
        }

        if (showSpace)
        {
            const bool supportsTransformSpace = m_GizmoMode == GizmoEditMode::Translate || m_GizmoMode == GizmoEditMode::Rotate;
            ImGui::SameLine();
            {
                UI::ScopedDisable disable(!supportsTransformSpace);
                const bool useLocalSpace = m_LocalMode || !supportsTransformSpace;
                if (HudButton(useLocalSpace ? "Local" : "World", !useLocalSpace, ImVec2(spaceWidth, buttonHeight)))
                    m_LocalMode = !m_LocalMode;
            }
            UI::SetTooltip(supportsTransformSpace ? "Change transform space" : "Scale and bounds use local space");
        }

        const bool modifierSnap = Input::IsKeyPressed(Key::LeftControl) || Input::IsKeyPressed(Key::RightControl);
        ImGui::SameLine();
        if (HudButton("Snap", m_SnapEnabled || modifierSnap, ImVec2(snapWidth, buttonHeight)))
            m_SnapEnabled = !m_SnapEnabled;
        UI::SetTooltip(m_SnapEnabled ? "Snapping is on. Click to turn it off" : "Click to keep snapping on. Hold Ctrl to use it temporarily");

        Ref<EditorSettings> editorSettings = Editor::Get().GetEditorSettings();
        if (showSnapValue)
        {
            char snapLabel[32];
            if (m_GizmoMode == GizmoEditMode::Rotate)
                snprintf(snapLabel, sizeof(snapLabel), "%.1f deg", editorSettings->GridRotateSnap);
            else if (m_GizmoMode == GizmoEditMode::Scale)
                snprintf(snapLabel, sizeof(snapLabel), "%.2f", editorSettings->GridScaleSnap);
            else
                snprintf(snapLabel, sizeof(snapLabel), "%.2f m", editorSettings->GridMoveSnap.x);

            ImGui::SameLine();
            if (ImGui::Button(snapLabel, ImVec2(valueWidth, buttonHeight)))
                ImGui::OpenPopup("##ViewportSnapSettings");
            UI::SetTooltip("Edit snap increments");
        }

        if (showHelp)
        {
            ImGui::SameLine();
            if (ImGui::Button("?", ImVec2(helpWidth, buttonHeight)))
                ImGui::OpenPopup("##ViewportNavigationHelp");
            UI::SetTooltip("Viewport controls");
        }

        if (ImGui::BeginPopup("##ViewportSnapSettings"))
        {
            m_MouseOverHud |= ImGui::IsWindowHovered();
            ImGui::TextDisabled("Snap increments");
            ImGui::Separator();
            ImGui::SetNextItemWidth(190.0f);
            if (ImGui::DragFloat3("Move", glm::value_ptr(editorSettings->GridMoveSnap), 0.01f, 0.001f, 1000.0f, "%.3f m"))
                editorSettings->GridMoveSnap = glm::max(editorSettings->GridMoveSnap, glm::vec3(0.001f));
            ImGui::SetNextItemWidth(190.0f);
            editorSettings->GridRotateSnap = std::max(editorSettings->GridRotateSnap, 0.1f);
            ImGui::DragFloat("Rotate", &editorSettings->GridRotateSnap, 0.5f, 0.1f, 180.0f, "%.1f deg");
            ImGui::SetNextItemWidth(190.0f);
            editorSettings->GridScaleSnap = std::max(editorSettings->GridScaleSnap, 0.001f);
            ImGui::DragFloat("Scale", &editorSettings->GridScaleSnap, 0.01f, 0.001f, 10.0f, "%.3f");
            ImGui::Spacing();
            ImGui::TextDisabled("Hold Ctrl for temporary snapping.");
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopup("##ViewportNavigationHelp"))
        {
            m_MouseOverHud |= ImGui::IsWindowHovered();
            ImGui::TextDisabled("Camera");
            ImGui::Separator();
            ImGui::TextUnformatted("Alt + left drag    Orbit");
            ImGui::TextUnformatted("Alt + middle drag  Pan");
            ImGui::TextUnformatted("Alt + right drag   Zoom");
            ImGui::TextUnformatted("Mouse wheel        Zoom");
            ImGui::TextUnformatted("F                  Frame selection");
            ImGui::EndPopup();
        }

        ImGui::PopStyleVar(3);
        ImGui::SetCursorScreenPos(savedCursor);

        if (viewportWidth >= 670.0f)
        {
            const EditorCamera& camera = EditorLayer::GetEditorCamera();
            const StringView entityName = selectedEntity ? StringView(selectedEntity.GetName()) : StringView{};
            const ViewportHudStatus status =
              FormatViewportHudStatus(entityName, static_cast<bool>(selectedEntity), selectedEntities.size(), static_cast<int32_t>(m_ViewportSize.x),
                                      static_cast<int32_t>(m_ViewportSize.y), camera.GetDistance());
            const ImVec2 textSize = ImGui::CalcTextSize(status.Text.data());
            const ImVec2 statusMin(imageMax.x - textSize.x - 16.0f, hudMin.y);
            const ImVec2 statusMax(imageMax.x - 8.0f, hudMax.y);
            if (statusMin.x > hudMax.x + 8.0f)
            {
                ImGui::GetWindowDrawList()->AddRectFilled(statusMin, statusMax, IM_COL32(27, 24, 22, 210), 5.0f);
                ImGui::GetWindowDrawList()->AddText(ImVec2(statusMin.x + 4.0f, statusMin.y + padding + ImGui::GetStyle().FramePadding.y),
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
            const ImVec2 statisticsMin(imageMax.x - textWidth - 24.0f, imageMin.y + 8.0f);
            const ImVec2 statisticsMax(imageMax.x - 8.0f, statisticsMin.y + lineHeight * lineCount + 16.0f);
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
                        const glm::mat4 pivot = GetSelectionPivot(selectedEntity, selectedEntities);
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
            else if (m_GizmoMode == GizmoEditMode::Scale)
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

                glm::mat4 transform = m_GizmoWasUsing                ? m_CurrentGizmoTransform
                                      : selectedEntities.size() > 1u ? GetSelectionPivot(selected, selectedEntities)
                                                                     : selected.GetWorldMatrix();
                const glm::mat4 originalTransform = transform;
                const bool supportsWorldSpace = m_GizmoMode == GizmoEditMode::Translate || m_GizmoMode == GizmoEditMode::Rotate;
                const ImGuizmo::OPERATION operation = m_GizmoMode == GizmoEditMode::Bounds ? ImGuizmo::SCALE : GetImGuizmoMode(m_GizmoMode);
                const bool manipulated = ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), operation,
                                                              !m_LocalMode && supportsWorldSpace ? ImGuizmo::WORLD : ImGuizmo::LOCAL,
                                                              glm::value_ptr(transform), nullptr, snap ? snapValues : nullptr);
                if (manipulated)
                {
                    if (!m_GizmoWasUsing)
                        BeginTransformInteraction(selectedEntities, originalTransform);
                    ApplyTransformInteraction(transform);
                }

                const bool usingGizmo = ImGuizmo::IsUsing();
                const TransformInteractionCompletion completion = ResolveTransformInteractionCompletion(
                  m_TransformTransaction.IsActive(), usingGizmo, Input::IsKeyPressed(Key::Escape));
                if (completion == TransformInteractionCompletion::Cancel)
                    CancelTransformInteraction();
                else if (completion == TransformInteractionCompletion::Commit)
                    EndTransformInteraction();
                // Keep the capture latched until ImGuizmo releases the mouse so
                // an Escape cancellation cannot begin a second transaction.
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
