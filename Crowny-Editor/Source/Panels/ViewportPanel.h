#pragma once

#include "Editor/BoxCollider2DBoundsTransaction.h"
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

    class ImGuiViewportSceneDraggedEvent : public Event
    {
    public:
        const FileEntry* GetFileEntry() const { return m_FileEntry; }
        const glm::vec2& GetScreenPosition() const { return m_ScreenPosition; }

        EVENT_CLASS_CATEGORY(EventCategoryImGui);
        EVENT_CLASS_TYPE(ImGuiViewportSceneDragged);

        ImGuiViewportSceneDraggedEvent(const FileEntry* fileEntry, const glm::vec2& screenPosition)
          : m_ScreenPosition(screenPosition), m_FileEntry(fileEntry)
        {
        }

    private:
        glm::vec2 m_ScreenPosition;
        const FileEntry* m_FileEntry;
    };

    /// Read/write access to the render-overlay state owned by the editor layer so the viewport's
    /// top-right toolbar and the Settings > Viewport checkboxes always agree.
    struct ViewportRenderOverlayBinding
    {
        std::function<bool()> IsWireframe;
        std::function<void(bool)> SetWireframe;
        std::function<bool()> IsShowingStatistics;
        std::function<void(bool)> SetShowStatistics;
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
        void SetEventCallback(const EventCallbackFn& onclicked);
        void SetEditorRenderTarget(const Ref<RenderTexture>& rt);
        void SetShowStatistics(bool show) { m_ShowStatistics = show; }
        void SetRenderOverlayBinding(ViewportRenderOverlayBinding binding) { m_RenderOverlayBinding = std::move(binding); }

        /// Handles files dropped from the OS shell. Returns true when the drop landed on the viewport image and
        /// was consumed (the files are imported into the project and, once imported, placed in the scene).
        bool OnWindowFileDrop(WindowFileDropEvent& fileDrop);
        size_t GetPendingDropSpawnCount() const { return m_PendingDropSpawns.size(); }

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

        bool IsMouseOverGizmo() const { return ImGuizmo::IsOver(); }
        bool IsMouseOverHud() const { return m_MouseOverHud; }

    private:
        struct PendingDropSpawn
        {
            Path AssetPath;
            glm::vec2 ScreenPosition;
            double QueuedAt = 0.0;
        };

        void DrawViewportHud(const ImVec2& imageMin, const ImVec2& imageMax, Entity primary, const Vector<Entity>& selectedEntities);
        void DrawRenderOverlayToolbar(const ImVec2& imageMin, const ImVec2& imageMax);
        void DrawRenderStatistics(const ImVec2& imageMin, const ImVec2& imageMax, float top);
        bool IsShowingStatistics() const;
        void ProcessPendingDropSpawns();
        void QueueDropSpawn(const Path& assetPath, const glm::vec2& screenPosition);
        const Vector<Entity>& RefreshSelectionScratch(Entity primary);
        void EndTransformInteraction();
        void CancelTransformInteraction();
        void EndColliderBoundsInteraction(bool cancel);
        void CancelActiveInteractions();

        bool m_LocalMode = true;
        bool m_SnapEnabled = false;
        bool m_ShowStatistics = true;
        bool m_MouseOverHud = false;
        Ref<RenderTexture> m_RenderTarget;
        EventCallbackFn OnEvent;
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
        bool m_GizmoWasUsing = false;
        ViewportRenderOverlayBinding m_RenderOverlayBinding;
        Vector<PendingDropSpawn> m_PendingDropSpawns;
        float m_TopRightOverlayBottom = 0.0f;
    };

} // namespace Crowny
