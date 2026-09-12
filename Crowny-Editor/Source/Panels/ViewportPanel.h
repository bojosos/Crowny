#pragma once

#include "Editor/BoxCollider2DBoundsTransaction.h"
#include "Editor/DecalBoundsInteraction.h"
#include "Editor/SceneGizmos.h"
#include "Editor/ViewportAssetDrop.h"
#include "Editor/ViewportTransformInteraction.h"
#include "Panels/EditorPanelRegistration.h"
#include "Panels/ImGuiPanel.h"

#include "Crowny/Ecs/Entity.h"
#include "Crowny/Events/ApplicationEvent.h"
#include "Crowny/Events/Event.h"
#include "Crowny/RenderAPI/RenderTexture.h"

#include <imgui.h>

#include <ImGuizmo.h>

#include <functional>
#include <optional>
#include <utility>

namespace Crowny
{
    struct FileEntry;

    enum class GizmoEditMode
    {
        None = 0,
        Translate = 1,
        Rotate = 2,
        Scale = 3,
        Bounds = 4
    };

    /// Read/write access to the render-overlay state owned by the editor layer so the viewport's
    /// top-right toolbar and the Settings > Viewport checkboxes always agree.
    struct ViewportRenderOverlayBinding
    {
        std::function<bool()> IsWireframe;
        std::function<void(bool)> SetWireframe;
        std::function<bool()> IsShowingStatistics;
        std::function<void(bool)> SetShowStatistics;
        std::function<uint32_t()> GetDecalDebugView;
        std::function<void(uint32_t)> SetDecalDebugView;
    };

    class ViewportPanel : public ImGuiPanel
    {
    public:
        inline static constexpr EditorPanelRegistration<ViewportPanel> Registration{ "Viewport", "View/Viewport" };

        ViewportPanel(const String& name, std::function<Entity()> selectedEntity, std::function<const Vector<Entity>&()> selectedEntities);
        ~ViewportPanel() override;

        virtual void Render() override;
        const glm::vec2& GetViewportSize() const { return m_ViewportSize; }
        const glm::vec4& GetViewportBounds() const { return m_ViewportBounds; }
        void SetDropActions(ViewportAssetDrop::Actions actions, std::function<Entity(const glm::vec2&)> pickEntity)
        {
            m_AssetDrops.SetActions(std::move(actions));
            m_PickDropEntity = std::move(pickEntity);
        }
        void SetEditorRenderTarget(const Ref<RenderTexture>& rt);
        void SetShowStatistics(bool show) { m_ShowStatistics = show; }
        void SetRenderOverlayBinding(ViewportRenderOverlayBinding binding) { m_RenderOverlayBinding = std::move(binding); }

        /// Handles files dropped from the OS shell. Returns true when the drop landed on the viewport image and
        /// was consumed (the files are imported into the project and, once imported, placed in the scene).
        bool OnWindowFileDrop(WindowFileDropEvent& fileDrop);
        size_t GetPendingDropSpawnCount() const { return m_AssetDrops.GetPendingCount(); }

        void SetGizmoMode(GizmoEditMode gizmoMode) { m_GizmoMode = gizmoMode; }
        void SetGizmoLocalMode(bool local) { m_LocalMode = local; }
        void SetSnapEnabled(bool enabled) { m_SnapEnabled = enabled; }
        void SetViewportSettingsCallbacks(std::function<void()> toggle, std::function<bool()> isOpen)
        {
            m_ToggleViewportSettings = std::move(toggle);
            m_IsViewportSettingsOpen = std::move(isOpen);
        }
        void SetViewportSettingsHovered(bool hovered) { m_MouseOverHud |= hovered; }

        bool GetGizmoLocalMode() const { return m_LocalMode; }
        bool GetSnapEnabled() const { return m_SnapEnabled; }
        GizmoEditMode GetGizmoMode() const { return m_GizmoMode; }

        void DisableGizmo() { m_GizmoMode = GizmoEditMode::None; }
        void EnableGizmo() { m_GizmoMode = GizmoEditMode::Translate; }

        bool IsMouseOverGizmo() const { return ImGuizmo::IsOver() || m_DecalBounds.IsUsing() || m_DecalHovered; }
        bool IsMouseOverHud() const { return m_MouseOverHud; }
        const SceneGizmoSettings& GetSceneGizmoSettings() const { return m_SceneGizmos; }

    private:
        void DrawViewportHud(const ImVec2& imageMin, const ImVec2& imageMax, Entity primary, const Vector<Entity>& selectedEntities);
        void DrawRenderOverlayToolbar(const ImVec2& imageMin, const ImVec2& imageMax);
        void DrawRenderStatistics(const ImVec2& imageMin, const ImVec2& imageMax, float top);
        bool IsShowingStatistics() const;
        void SubmitDrop(const Path& path, const glm::vec2& screenPosition);
        std::optional<glm::vec3> GetDropPosition(const glm::vec2& screenPosition) const;
        const Vector<Entity>& RefreshSelectionScratch(Entity primary);
        void EndTransformInteraction();
        void CancelTransformInteraction();
        void EndColliderBoundsInteraction(bool cancel);
        void CancelActiveInteractions();

        bool m_LocalMode = true;
        bool m_SnapEnabled = false;
        bool m_ShowStatistics = true;
        bool m_MouseOverHud = false;
        SceneGizmoSettings m_SceneGizmos;
        Ref<RenderTexture> m_RenderTarget;
        ViewportAssetDrop m_AssetDrops = CreateProjectViewportAssetDrop();
        std::function<Entity(const glm::vec2&)> m_PickDropEntity;
        GizmoEditMode m_GizmoMode = GizmoEditMode::Translate;
        glm::vec2 m_ViewportSize = { 1.0f, 1.0f };
        glm::vec4 m_ViewportBounds;
        std::function<Entity()> m_SelectedEntity;
        std::function<const Vector<Entity>&()> m_SelectedEntities;
        std::function<void()> m_ToggleViewportSettings;
        std::function<bool()> m_IsViewportSettingsOpen;
        Vector<Entity> m_SelectedEntitiesScratch;
        ViewportTransformInteraction m_TransformInteraction;
        BoxCollider2DBoundsTransaction m_ColliderBoundsTransaction;
        DecalBoundsInteraction m_DecalBounds;
        bool m_DecalHovered = false;
        bool m_GizmoWasUsing = false;
        ViewportRenderOverlayBinding m_RenderOverlayBinding;
        float m_TopRightOverlayBottom = 0.0f;
    };

} // namespace Crowny
