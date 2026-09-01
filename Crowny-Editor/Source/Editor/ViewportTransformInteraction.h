#pragma once

#include "Editor/UndoRedo.h"

#include "Crowny/Ecs/Entity.h"

#include <cstdint>

namespace Crowny
{
    enum class SelectionTransformOperation : uint8_t
    {
        Translate,
        Rotate,
        Scale
    };

    enum class SelectionTransformSpace : uint8_t
    {
        World,
        Local
    };

    enum class TransformInteractionCompletion : uint8_t
    {
        None,
        Commit,
        Cancel
    };

    constexpr TransformInteractionCompletion ResolveTransformInteractionCompletion(bool interactionActive, bool gizmoUsing, bool cancelRequested)
    {
        if (!interactionActive)
            return TransformInteractionCompletion::None;
        if (cancelRequested)
            return TransformInteractionCompletion::Cancel;
        return gizmoUsing ? TransformInteractionCompletion::None : TransformInteractionCompletion::Commit;
    }

    struct ViewportTransformResolution
    {
        TransformInteractionCompletion State = TransformInteractionCompletion::None;
        Ref<UndoAction> Action;
    };

    struct ViewportGizmoFrame
    {
        glm::mat4 Pivot{ 1.0f };
        bool Manipulated = false;
        bool Using = false;
        bool CancelRequested = false;
    };

    struct ViewportTransformObservedState
    {
        bool Valid = false;
        glm::vec3 WorldPosition{ 0.0f };
        glm::vec3 LocalPosition{ 0.0f };
    };

    struct ViewportTransformFrameResult
    {
        bool Manipulated = false;
        bool GizmoUsing = false;
        bool CancelRequested = false;
        bool BeginAttempted = false;
        bool Began = false;
        bool UpdateAttempted = false;
        bool Updated = false;
        bool BlockedUntilRelease = false;
        ViewportTransformObservedState Before;
        ViewportTransformObservedState After;
        ViewportTransformResolution Resolution;
    };

    class ViewportTransformInteraction
    {
    public:
        ViewportTransformInteraction() = default;
        ~ViewportTransformInteraction();
        ViewportTransformInteraction(const ViewportTransformInteraction&) = delete;
        ViewportTransformInteraction& operator=(const ViewportTransformInteraction&) = delete;

        // The pivot always has unit scale. Its orientation is world-aligned or follows
        // the top-level primary target, making the gizmo matrix a shared selection frame.
        static glm::mat4 CalculatePivot(const Vector<Entity>& targets, Entity primary, SelectionTransformSpace space);

        bool Begin(const Vector<Entity>& targets, Entity primary, SelectionTransformOperation operation, SelectionTransformSpace space);
        bool Update(const glm::mat4& pivot);
        ViewportTransformFrameResult ProcessGizmoFrame(const Vector<Entity>& targets, Entity primary, SelectionTransformOperation operation,
                                                       SelectionTransformSpace space, const ViewportGizmoFrame& frame);
        ViewportTransformResolution Resolve(bool gizmoUsing, bool cancelRequested);
        void Cancel();

        bool IsActive() const { return m_Transaction.IsActive(); }
        const glm::mat4& GetCurrentPivot() const { return m_CurrentPivot; }

    private:
        struct Snapshot
        {
            Ref<Scene> SceneRef;
            UUID Target = UUID::EMPTY;
            glm::mat4 WorldTransform{ 1.0f };
            glm::vec3 WorldPosition{ 0.0f };
            glm::quat WorldRotation{ 1.0f, 0.0f, 0.0f, 0.0f };
            glm::vec3 WorldScale{ 1.0f };
            bool PositionOverridden = false;
            bool RotationOverridden = false;
            bool ScaleOverridden = false;
        };

        static Vector<Entity> NormalizeTargets(const Vector<Entity>& targets);
        static bool MatrixChanged(const glm::mat4& lhs, const glm::mat4& rhs);
        static Entity ResolveTarget(const Snapshot& snapshot);
        static ViewportTransformObservedState Observe(Entity entity);

        bool ApplyTranslation(const glm::vec3& pivotPosition);
        bool ApplyRotation(const glm::vec3& pivotPosition, const glm::quat& pivotRotation);
        bool ApplyScale(const glm::vec3& pivotPosition, const glm::vec3& pivotScale);

        Ref<UndoAction> Commit();
        Ref<UndoAction> BuildAction() const;
        void Reset();

        static constexpr UndoTransaction::Id TransactionId = 1u;
        UndoTransaction m_Transaction;
        Vector<Snapshot> m_Snapshots;
        SelectionTransformOperation m_Operation = SelectionTransformOperation::Translate;
        glm::mat4 m_InitialPivot{ 1.0f };
        glm::mat4 m_CurrentPivot{ 1.0f };
        glm::vec3 m_InitialPivotPosition{ 0.0f };
        glm::quat m_InitialPivotRotation{ 1.0f, 0.0f, 0.0f, 0.0f };
        bool m_BlockUntilRelease = false;
    };
} // namespace Crowny
