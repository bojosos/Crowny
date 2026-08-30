#pragma once

#include "Editor/UndoRedo.h"

#include "Crowny/Ecs/Entity.h"

#include <cstdint>

namespace Crowny
{
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

    class ViewportTransformInteraction
    {
    public:
        ViewportTransformInteraction() = default;
        ~ViewportTransformInteraction();
        ViewportTransformInteraction(const ViewportTransformInteraction&) = delete;
        ViewportTransformInteraction& operator=(const ViewportTransformInteraction&) = delete;

        bool Begin(const Vector<Entity>& targets, const glm::mat4& pivot);
        bool Update(const glm::mat4& pivot);
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
        };

        static bool MatrixChanged(const glm::mat4& lhs, const glm::mat4& rhs);
        static Entity ResolveTarget(const Snapshot& snapshot);

        Ref<UndoAction> Commit();
        Ref<UndoAction> BuildAction() const;
        void Reset();

        static constexpr UndoTransaction::Id TransactionId = 1u;
        UndoTransaction m_Transaction;
        Vector<Snapshot> m_Snapshots;
        glm::mat4 m_InitialPivot{ 1.0f };
        glm::mat4 m_CurrentPivot{ 1.0f };
    };
} // namespace Crowny
