#pragma once

#include "Editor/BoxCollider2DBoundsTransaction.h"
#include "Editor/ViewportTransformInteraction.h"
#include "Panels/EditorPanelRegistration.h"
#include "Panels/ImGuiPanel.h"

#include "Crowny/Ecs/Entity.h"
#include "Crowny/Events/Event.h"
#include "Crowny/RenderAPI/RenderTexture.h"

#include <imgui.h>

#include <ImGuizmo.h>

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

        void SetGizmoMode(GizmoEditMode gizmoMode) { m_GizmoMode = gizmoMode; }
        void SetGizmoLocalMode(bool local) { m_LocalMode = local; }

        bool GetGizmoLocalMode() const { return m_LocalMode; }
        GizmoEditMode GetGizmoMode() const { return m_GizmoMode; }

        void DisableGizmo() { m_GizmoMode = GizmoEditMode::None; }
        void EnableGizmo() { m_GizmoMode = GizmoEditMode::Translate; }

        bool IsMouseOverGizmo() const { return ImGuizmo::IsOver(); }
        bool IsMouseOverHud() const { return m_MouseOverHud; }

    private:
        void DrawViewportHud(const ImVec2& imageMin, const ImVec2& imageMax, Entity primary, const Vector<Entity>& selectedEntities);
        const Vector<Entity>& RefreshSelectionScratch(Entity primary);
        const Vector<Entity>& GetTopLevelSelection(const Vector<Entity>& selectedEntities);
        glm::mat4 GetSelectionPivot(Entity primary, const Vector<Entity>& selectedEntities) const;
        void BeginTransformInteraction(const Vector<Entity>& selectedEntities, const glm::mat4& pivot);
        void ApplyTransformInteraction(const glm::mat4& pivot);
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
        Vector<Entity> m_SelectedEntitiesScratch;
        Vector<Entity> m_TopLevelSelectionScratch;
        ViewportTransformInteraction m_TransformInteraction;
        BoxCollider2DBoundsTransaction m_ColliderBoundsTransaction;
        bool m_GizmoWasUsing = false;
    };

} // namespace Crowny
