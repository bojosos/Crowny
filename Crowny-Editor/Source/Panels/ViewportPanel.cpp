#include "cwepch.h"

#include "Editor/Editor.h"
#include "Editor/EditorLayer.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Events/ImGuiEvent.h"
#include "Crowny/Input/Input.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/Scene/SceneRenderer.h"

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
              : UndoAction("Transform entity"), m_Scene(target.GetScene()), m_Target(target.GetUuid()),
                m_OldTransform(oldTransform), m_NewTransform(newTransform)
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

            Scene* m_Scene = nullptr;
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

    ViewportPanel::ViewportPanel(const String& name, std::function<Entity()> selectedEntity, std::function<Vector<Entity>()> selectedEntities)
      : ImGuiPanel(name), m_ViewportBounds(0.0f), m_SelectedEntity(std::move(selectedEntity)), m_SelectedEntities(std::move(selectedEntities))
    {
        Ref<ProjectSettings> projSettings = Editor::Get().GetProjectSettings();
        m_GizmoMode = projSettings->GizmoMode;
        m_LocalMode = projSettings->GizmoLocalMode;
    }

    Vector<Entity> ViewportPanel::GetTopLevelSelection(const Vector<Entity>& selectedEntities) const
    {
        Vector<Entity> result;
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
                result.push_back(entity);
        }
        return result;
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
        m_TransformSnapshots.clear();
        for (Entity entity : GetTopLevelSelection(selectedEntities))
            m_TransformSnapshots.push_back({ entity, entity.GetWorldMatrix() });
        m_InitialGizmoTransform = pivot;
        m_CurrentGizmoTransform = pivot;
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
    }

    void ViewportPanel::EndTransformInteraction()
    {
        Ref<UndoActionGroup> actions =
          CreateRef<UndoActionGroup>(m_TransformSnapshots.size() == 1u ? "Transform entity" : "Transform entities");
        for (const TransformSnapshot& snapshot : m_TransformSnapshots)
        {
            if (!snapshot.Target)
                continue;
            const glm::mat4 current = snapshot.Target.GetWorldMatrix();
            if (MatrixChanged(snapshot.WorldTransform, current))
                actions->Add(CreateRef<WorldTransformAction>(snapshot.Target, snapshot.WorldTransform, current));
        }
        if (!actions->Empty())
            UndoRedo::Get().RegisterAction(actions);
        m_TransformSnapshots.clear();
    }

    void ViewportPanel::DrawViewportHud(const ImVec2& imageMin, const ImVec2& imageMax, Entity selectedEntity,
                                        const Vector<Entity>& selectedEntities)
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
            String entityName = selectedEntities.size() > 1u ? fmt::format("{} entities", selectedEntities.size())
                                                              : selectedEntity ? selectedEntity.GetName() : "No selection";
            if (entityName.size() > 28)
                entityName = entityName.substr(0, 25) + "...";
            const EditorCamera& camera = EditorLayer::GetEditorCamera();
            const String status = fmt::format("{}  |  {} x {}  |  View {:.1f} m", entityName, static_cast<int32_t>(m_ViewportSize.x),
                                              static_cast<int32_t>(m_ViewportSize.y), camera.GetDistance());
            const ImVec2 textSize = ImGui::CalcTextSize(status.c_str());
            const ImVec2 statusMin(imageMax.x - textSize.x - 16.0f, hudMin.y);
            const ImVec2 statusMax(imageMax.x - 8.0f, hudMax.y);
            if (statusMin.x > hudMax.x + 8.0f)
            {
                ImGui::GetWindowDrawList()->AddRectFilled(statusMin, statusMax, IM_COL32(27, 24, 22, 210), 5.0f);
                ImGui::GetWindowDrawList()->AddText(ImVec2(statusMin.x + 4.0f, statusMin.y + padding + ImGui::GetStyle().FramePadding.y),
                                                    IM_COL32(190, 184, 178, 255), status.c_str());
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
            snprintf(lines[1], sizeof(lines[1]), "Draws %llu   Verts %s   Tris %s",
                     static_cast<unsigned long long>(frame.DrawCalls), vertexCount, triangleCount);
            snprintf(lines[2], sizeof(lines[2]), "Visible %u / %u   Lights %u", scene.VisibleInstances,
                     scene.ActiveInstances, scene.ActiveLights);
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
                ImGui::Text("Graph: %u graphics, %u compute, %u transfer", scene.GraphicsPasses, scene.ComputePasses,
                            scene.TransferPasses);
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
                    Vector<Entity> selectedEntities = m_SelectedEntities ? m_SelectedEntities() : Vector<Entity>{};
                    if (selectedEntities.empty() && selectedEntity)
                        selectedEntities.push_back(selectedEntity);
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
        RenderTexture* rt = static_cast<RenderTexture*>(m_RenderTarget.get());
        Ref<Texture> texture = rt->GetColorTexture(0);

        ImTextureID textureID = ImGuiVulkanTexture::Get(texture);
        ImGui::Image(textureID, ImVec2(m_ViewportSize.x, m_ViewportSize.y), ImVec2{ 0, 1 }, ImVec2{ 1, 0 }); // The viewport itself

        const ImVec2 imageMin = ImGui::GetItemRectMin();
        const ImVec2 imageMax = ImGui::GetItemRectMax();
        m_ViewportBounds = { imageMin.x, imageMin.y, imageMax.x, imageMax.y };

        if (ImGui::BeginDragDropTarget())
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
                ImVec2 mouseCoords = ImGui::GetMousePos();
                glm::vec2 coords = { mouseCoords.x - imageMin.x, mouseCoords.y - imageMin.y };
                coords.y = m_ViewportSize.y - coords.y - 1;

                ImGuiViewportSceneDraggedEvent fileDragEvent(fileEntry, coords);
                if (OnEvent)
                    OnEvent(fileDragEvent);
            }
            ImGui::EndDragDropTarget();
        }

        Entity selected = m_SelectedEntity ? m_SelectedEntity() : Entity{};
        Vector<Entity> selectedEntities = m_SelectedEntities ? m_SelectedEntities() : Vector<Entity>{};
        selectedEntities.erase(std::remove_if(selectedEntities.begin(), selectedEntities.end(), [](Entity entity) { return !entity.IsValid(); }),
                               selectedEntities.end());
        if (selected && std::find(selectedEntities.begin(), selectedEntities.end(), selected) == selectedEntities.end())
            selectedEntities.push_back(selected);

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
                        bc2d.SetOffset(newOffset, selected);
                        bc2d.SetSize(newSize, selected);
                    }
                }
            }
            else
            {
                glm::mat4 transform = m_GizmoWasUsing
                                        ? m_CurrentGizmoTransform
                                        : selectedEntities.size() > 1u ? GetSelectionPivot(selected, selectedEntities) : selected.GetWorldMatrix();
                const glm::mat4 originalTransform = transform;
                const bool supportsWorldSpace = m_GizmoMode == GizmoEditMode::Translate || m_GizmoMode == GizmoEditMode::Rotate;
                const ImGuizmo::OPERATION operation = m_GizmoMode == GizmoEditMode::Bounds ? ImGuizmo::SCALE : GetImGuizmoMode(m_GizmoMode);
                const bool manipulated = ImGuizmo::Manipulate(
                  glm::value_ptr(view), glm::value_ptr(proj), operation,
                  !m_LocalMode && supportsWorldSpace ? ImGuizmo::WORLD : ImGuizmo::LOCAL, glm::value_ptr(transform), nullptr,
                  snap ? snapValues : nullptr);
                if (manipulated)
                {
                    if (!m_GizmoWasUsing)
                        BeginTransformInteraction(selectedEntities, originalTransform);
                    ApplyTransformInteraction(transform);
                }

                const bool usingGizmo = ImGuizmo::IsUsing();
                if (m_GizmoWasUsing && !usingGizmo)
                    EndTransformInteraction();
                m_GizmoWasUsing = usingGizmo;
            }
        }
        else if (m_GizmoWasUsing)
        {
            EndTransformInteraction();
            m_GizmoWasUsing = false;
        }
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

    void ViewportPanel::SetEditorRenderTarget(const Ref<RenderTarget>& rt) { m_RenderTarget = rt; }

} // namespace Crowny
