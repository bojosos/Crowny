#include "cwepch.h"

#include "Editor/Editor.h"
#include "Editor/EditorAssets.h"
#include "Editor/EditorLayer.h"
#include "Editor/ProjectLibrary.h"
#include "Editor/ViewportTransformInteraction.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Events/ImGuiEvent.h"
#include "Crowny/Input/Input.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/RenderAPI/Texture.h"
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
        // Mesh sources (.obj/.gltf/.glb/.fbx/...) that have not produced metadata yet, e.g. because the
        // library has not imported them, are still accepted: the drop triggers the import and then spawns.
        bool IsPendingMeshSource(const FileEntry* fileEntry)
        {
            return fileEntry != nullptr && fileEntry->Metadata == nullptr && IsViewportMeshExtension(fileEntry->Filepath.extension().string());
        }

        bool IsSupportedViewportAsset(const FileEntry* fileEntry)
        {
            if (fileEntry == nullptr)
                return false;
            if (fileEntry->Metadata == nullptr)
                return IsPendingMeshSource(fileEntry);

            const AssetType assetType = fileEntry->Metadata->Type;
            return assetType == AssetType::Scene || assetType == AssetType::Material || assetType == AssetType::Mesh ||
                   assetType == AssetType::AudioClip || assetType == AssetType::Prefab;
        }

        const char* GetViewportDropLabel(const FileEntry* fileEntry)
        {
            if (!IsSupportedViewportAsset(fileEntry))
                return "This asset cannot be used in the viewport";
            if (fileEntry->Metadata == nullptr)
                return "Drop to import and create mesh entity";

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

        bool SameFileContents(const Path& left, const Path& right)
        {
            std::error_code error;
            const uintmax_t leftSize = fs::file_size(left, error);
            if (error)
                return false;
            const uintmax_t rightSize = fs::file_size(right, error);
            return !error && leftSize == rightSize;
        }

        // Picks a destination inside the asset folder for an external file. Re-uses an existing copy with the same
        // size (dropping the same model twice just adds another instance) and otherwise appends " (n)".
        Path ResolveDropDestination(const Path& source, const Path& assetFolder)
        {
            Path destination = assetFolder / source.filename();
            if (!fs::exists(destination) || SameFileContents(source, destination))
                return destination;

            const String stem = source.stem().string();
            const String extension = source.extension().string();
            for (uint32_t index = 1; index < 1000; index++)
            {
                destination = assetFolder / (stem + " (" + std::to_string(index) + ")" + extension);
                if (!fs::exists(destination) || SameFileContents(source, destination))
                    return destination;
            }
            return {};
        }

        bool CopyDropFile(const Path& source, const Path& destination)
        {
            if (fs::exists(destination) && SameFileContents(source, destination))
                return true;
            std::error_code error;
            fs::create_directories(destination.parent_path(), error);
            error.clear();
            fs::copy_file(source, destination, fs::copy_options::overwrite_existing, error);
            if (error)
            {
                CW_ENGINE_ERROR("Failed to import dropped file '{}' into '{}': {}", source, destination, error.message());
                return false;
            }
            return true;
        }

        // Copies the external file (plus glTF buffers/images and OBJ material libraries it references) into the
        // project's asset folder. Files already inside the asset folder are used in place. Returns the in-project path.
        Path ImportExternalDropFile(const Path& source, const Path& assetFolder)
        {
            std::error_code error;
            if (!fs::is_regular_file(source, error) || error)
                return {};

            const Path normalizedSource = source.lexically_normal();
            if (AssetFileSystemScanner::IsPathWithin(assetFolder, normalizedSource))
                return normalizedSource;

            const Path destination = ResolveDropDestination(normalizedSource, assetFolder);
            if (destination.empty() || !CopyDropFile(normalizedSource, destination))
                return {};

            if (ClassifyViewportDropFile(normalizedSource) == ViewportDropFileKind::Mesh)
            {
                const String contents = FileSystem::ReadTextFile(normalizedSource);
                for (const Path& reference : CollectMeshSidecarReferences(normalizedSource, contents))
                {
                    const Path sidecarSource = (normalizedSource.parent_path() / reference).lexically_normal();
                    const Path sidecarDestination = (destination.parent_path() / reference).lexically_normal();
                    if (!AssetFileSystemScanner::IsPathWithin(assetFolder, sidecarDestination) || !fs::is_regular_file(sidecarSource, error) || error)
                    {
                        error.clear();
                        CW_ENGINE_WARN("Dropped mesh '{}' references '{}', which could not be copied alongside it.", normalizedSource, reference);
                        continue;
                    }
                    CopyDropFile(sidecarSource, sidecarDestination);
                }
            }
            return destination;
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
            ImRect iconBounds(ImVec2(bounds.Min.x + iconPadding, bounds.Min.y + iconPadding),
                              ImVec2(bounds.Max.x - iconPadding, bounds.Max.y - iconPadding));
            // Letterbox the icon inside the square button so non-square source textures keep their aspect ratio
            // instead of being stretched to the button.
            if (icon && icon->GetWidth() > 0 && icon->GetHeight() > 0)
            {
                const float aspect = static_cast<float>(icon->GetWidth()) / static_cast<float>(icon->GetHeight());
                float width = iconBounds.GetWidth();
                float height = iconBounds.GetHeight();
                if (aspect > 1.0f)
                    height = width / aspect;
                else
                    width = height * aspect;
                const ImVec2 center = iconBounds.GetCenter();
                iconBounds = ImRect(ImVec2(center.x - width * 0.5f, center.y - height * 0.5f), ImVec2(center.x + width * 0.5f, center.y + height * 0.5f));
            }
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

        DrawRenderOverlayToolbar(imageMin, imageMax);

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

        // Statistics live top-left under the tool strip so they never collide with the render toolbar or the view cube.
        DrawRenderStatistics(imageMin, imageMax, hudMax.y + 6.0f);
    }

    bool ViewportPanel::IsShowingStatistics() const
    {
        return m_RenderOverlayBinding.IsShowingStatistics ? m_RenderOverlayBinding.IsShowingStatistics() : m_ShowStatistics;
    }

    void ViewportPanel::DrawRenderOverlayToolbar(const ImVec2& imageMin, const ImVec2& imageMax)
    {
        m_TopRightOverlayBottom = imageMin.y;
        const float viewportWidth = imageMax.x - imageMin.x;
        if (viewportWidth < 420.0f)
            return;

        const bool canToggleWireframe = static_cast<bool>(m_RenderOverlayBinding.IsWireframe) && static_cast<bool>(m_RenderOverlayBinding.SetWireframe);
        const bool wireframe = canToggleWireframe && m_RenderOverlayBinding.IsWireframe();
        const bool showStatistics = IsShowingStatistics();

        const float padding = 4.0f;
        const float spacing = 2.0f;
        const float buttonSize = 22.0f;
        const float textPadding = 8.0f;
        const char* renderModeLabel = wireframe ? "Wireframe" : "Shaded";
        const float renderModeWidth = ImGui::CalcTextSize(renderModeLabel).x + ImGui::CalcTextSize(" v").x + textPadding * 2.0f;
        const float statisticsWidth = ImGui::CalcTextSize("Stats").x + textPadding * 2.0f;
        const float separatorGap = 5.0f;
        const float toolbarWidth = padding * 2.0f + renderModeWidth + separatorGap + statisticsWidth;
        const ImVec2 toolbarMin(imageMax.x - toolbarWidth - 8.0f, imageMin.y + 5.0f);
        const ImVec2 toolbarMax(toolbarMin.x + toolbarWidth, toolbarMin.y + buttonSize + padding * 2.0f);
        m_TopRightOverlayBottom = toolbarMax.y;
        m_MouseOverHud |= ImGui::IsMouseHoveringRect(toolbarMin, toolbarMax, false);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(toolbarMin, toolbarMax, IM_COL32(27, 24, 22, 220), 4.0f);
        drawList->AddRect(toolbarMin, toolbarMax, IM_COL32(90, 81, 73, 180), 4.0f);

        const ImVec2 savedCursor = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(toolbarMin.x + padding, toolbarMin.y + padding));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, 0.0f));
        ImGui::PushID("ViewportRenderToolbar");

        const ImU32 neutralText = IM_COL32(190, 184, 178, 255);
        const ImU32 hoverText = IM_COL32(235, 232, 228, 255);
        const auto drawTextButton = [&](const char* id, const char* label, float width, bool active, bool enabled, StringView tooltip) {
            ImGui::PushID(id);
            ImGui::BeginDisabled(!enabled);
            const bool clicked = ImGui::InvisibleButton("##text", ImVec2(width, buttonSize));
            ImGui::EndDisabled();
            const ImRect bounds = UI::GetItemRect();
            const bool hovered = enabled && ImGui::IsItemHovered();
            if (active)
                drawList->AddRectFilled(bounds.Min, bounds.Max, IM_COL32(196, 123, 48, 42), 3.0f);
            else if (hovered)
                drawList->AddRectFilled(bounds.Min, bounds.Max, IM_COL32(255, 255, 255, 12), 3.0f);
            const ImU32 textColor = !enabled ? IM_COL32(120, 112, 105, 255) : active ? UI::Colors::Accent : hovered ? hoverText : neutralText;
            const ImVec2 textSize = ImGui::CalcTextSize(label);
            drawList->AddText(ImVec2(bounds.Min.x + (bounds.GetWidth() - textSize.x) * 0.5f, bounds.Min.y + (bounds.GetHeight() - textSize.y) * 0.5f),
                              textColor, label);
            UI::SetTooltip(tooltip);
            ImGui::PopID();
            return clicked;
        };

        char renderModeButton[32];
        snprintf(renderModeButton, sizeof(renderModeButton), "%s v", renderModeLabel);
        const ImVec2 renderModeButtonMin = ImGui::GetCursorScreenPos();
        if (drawTextButton("RenderMode", renderModeButton, renderModeWidth, wireframe, canToggleWireframe,
                           canToggleWireframe ? "Draw mode: Shaded or Wireframe (also in Settings > Viewport)" : "Draw mode is unavailable"))
            ImGui::OpenPopup("##ViewportRenderModePopup");

        ImGui::SameLine(0.0f, separatorGap);
        const ImVec2 separatorTop = ImGui::GetCursorScreenPos();
        drawList->AddLine(ImVec2(separatorTop.x - separatorGap * 0.5f, separatorTop.y + 3.0f),
                          ImVec2(separatorTop.x - separatorGap * 0.5f, separatorTop.y + buttonSize - 3.0f), IM_COL32(104, 94, 85, 150));
        if (drawTextButton("Statistics", "Stats", statisticsWidth, showStatistics, true,
                           showStatistics ? "Hide rendering statistics (also in Settings > Viewport)" : "Show rendering statistics (also in Settings > Viewport)"))
        {
            if (m_RenderOverlayBinding.SetShowStatistics)
                m_RenderOverlayBinding.SetShowStatistics(!showStatistics);
            m_ShowStatistics = !showStatistics;
        }

        ImGui::SetNextWindowPos(ImVec2(renderModeButtonMin.x, toolbarMax.y + 2.0f));
        if (ImGui::BeginPopup("##ViewportRenderModePopup"))
        {
            m_MouseOverHud |= ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByPopup);
            if (ImGui::MenuItem("Shaded", nullptr, !wireframe) && canToggleWireframe)
                m_RenderOverlayBinding.SetWireframe(false);
            if (ImGui::MenuItem("Wireframe", nullptr, wireframe) && canToggleWireframe)
                m_RenderOverlayBinding.SetWireframe(true);
            ImGui::EndPopup();
        }

        ImGui::PopID();
        ImGui::PopStyleVar();
        ImGui::SetCursorScreenPos(savedCursor);
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
    }

    void ViewportPanel::DrawRenderStatistics(const ImVec2& imageMin, const ImVec2& imageMax, float top)
    {
        const float viewportWidth = imageMax.x - imageMin.x;
        const float viewportHeight = imageMax.y - imageMin.y;
        if (IsShowingStatistics() && viewportWidth >= 260.0f && viewportHeight >= 120.0f && RenderAPI::TryGet() != nullptr)
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
            const ImVec2 statisticsMin(imageMin.x + 6.0f, top);
            const ImVec2 statisticsMax(statisticsMin.x + textWidth + 16.0f, statisticsMin.y + lineHeight * lineCount + 16.0f);
            if (statisticsMax.y > imageMax.y - 8.0f || statisticsMax.x > imageMax.x - 8.0f)
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
        ProcessPendingDropSpawns();
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
                if (IsPendingMeshSource(fileEntry))
                {
                    // Not imported yet (e.g. a .gltf/.glb the library skipped): import now, spawn once metadata exists.
                    if (ProjectLibrary::TryGet() != nullptr)
                        ProjectLibrary::Get().Reimport(fileEntry->Filepath);
                    QueueDropSpawn(fileEntry->Filepath, glm::vec2(mousePosition.x, mousePosition.y));
                }
                else
                {
                    ImGuiViewportSceneDraggedEvent fileDragEvent(fileEntry, glm::vec2(mousePosition.x, mousePosition.y));
                    if (OnEvent)
                        OnEvent(fileDragEvent);
                }
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
        // The view cube sits below the top-right render toolbar (when that toolbar is visible).
        const float viewCubeTop = m_TopRightOverlayBottom > imageMin.y ? m_TopRightOverlayBottom + 4.0f : imageMin.y + 4.0f;
        if (viewportMinSize >= 100.0f &&
            ImGuizmo::ViewManipulate(glm::value_ptr(view), camera.GetDistance(), { imageMax.x - viewCubeSize - 8.0f, viewCubeTop },
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

    void ViewportPanel::QueueDropSpawn(const Path& assetPath, const glm::vec2& screenPosition)
    {
        m_PendingDropSpawns.push_back(PendingDropSpawn{ assetPath.lexically_normal(), screenPosition, ImGui::GetTime() });
    }

    void ViewportPanel::ProcessPendingDropSpawns()
    {
        if (m_PendingDropSpawns.empty())
            return;
        if (ProjectLibrary::TryGet() == nullptr)
        {
            m_PendingDropSpawns.clear();
            return;
        }

        constexpr double ImportTimeoutSeconds = 60.0;
        const double now = ImGui::GetTime();
        const bool importing = ProjectLibrary::Get().IsImporting();
        for (size_t index = 0; index < m_PendingDropSpawns.size();)
        {
            const PendingDropSpawn& pending = m_PendingDropSpawns[index];
            const Ref<LibraryEntry> entry = ProjectLibrary::Get().FindEntry(pending.AssetPath);
            const FileEntry* fileEntry =
              entry != nullptr && entry->Type == LibraryEntryType::File ? static_cast<const FileEntry*>(entry.get()) : nullptr;
            if (fileEntry != nullptr && fileEntry->Metadata != nullptr)
            {
                if (IsSupportedViewportAsset(fileEntry))
                {
                    ImGuiViewportSceneDraggedEvent fileDragEvent(fileEntry, pending.ScreenPosition);
                    if (OnEvent)
                        OnEvent(fileDragEvent);
                }
                m_PendingDropSpawns.erase(m_PendingDropSpawns.begin() + static_cast<std::ptrdiff_t>(index));
                continue;
            }

            const double waited = now - pending.QueuedAt;
            // Give the scheduler a moment to pick the file up; afterwards an idle importer means the import failed.
            const bool importFailed = !importing && waited > 2.0 && fileEntry != nullptr;
            if (importFailed || waited > ImportTimeoutSeconds)
            {
                CW_ENGINE_WARN("Dropped file '{}' was not imported; nothing was added to the scene.", pending.AssetPath);
                m_PendingDropSpawns.erase(m_PendingDropSpawns.begin() + static_cast<std::ptrdiff_t>(index));
                continue;
            }
            index++;
        }
    }

    bool ViewportPanel::OnWindowFileDrop(WindowFileDropEvent& fileDrop)
    {
        if (!IsShown() || !m_RenderTarget || fileDrop.GetPaths().empty())
            return false;

        // GLFW reports the drop in window-client pixels; ImGui works in the main viewport's coordinate space.
        const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        const glm::vec2 screenPosition =
          fileDrop.GetMousePosition() + (mainViewport != nullptr ? glm::vec2(mainViewport->Pos.x, mainViewport->Pos.y) : glm::vec2(0.0f));
        const bool insideViewport = screenPosition.x >= m_ViewportBounds.x && screenPosition.x < m_ViewportBounds.z &&
                                    screenPosition.y >= m_ViewportBounds.y && screenPosition.y < m_ViewportBounds.w;
        if (!insideViewport)
            return false;

        if (Editor::TryGet() == nullptr || !Editor::Get().IsProjectLoaded() || ProjectLibrary::TryGet() == nullptr)
        {
            CW_ENGINE_WARN("Ignoring dropped files: no project is loaded.");
            return true;
        }

        const Path& assetFolder = ProjectLibrary::Get().GetAssetFolder();
        bool imported = false;
        for (const Path& source : fileDrop.GetPaths())
        {
            if (ClassifyViewportDropFile(source) == ViewportDropFileKind::Unsupported)
            {
                CW_ENGINE_WARN("Dropped file '{}' is not a supported asset type.", source);
                continue;
            }
            const Path destination = ImportExternalDropFile(source, assetFolder);
            if (destination.empty())
                continue;
            QueueDropSpawn(destination, screenPosition);
            imported = true;
        }

        if (imported)
            ProjectLibrary::Get().RefreshAsync(assetFolder);
        return true;
    }

    void ViewportPanel::SetEditorRenderTarget(const Ref<RenderTexture>& rt) { m_RenderTarget = rt; }

} // namespace Crowny
