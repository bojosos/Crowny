#include "cwepch.h"

#include "Editor/ViewportTransformInteraction.h"

#include "Crowny/Scene/Scene.h"

namespace Crowny
{
    namespace
    {
        constexpr float TransformEpsilon = 0.00001f;

        class WorldTransformAction final : public UndoAction
        {
        public:
            WorldTransformAction(Entity target, const glm::mat4& before, const glm::mat4& after)
              : UndoAction("Transform entity"), m_Scene(target.GetScene()), m_Target(target.GetUuid()), m_Before(before), m_After(after)
            {
            }

            void Commit() override { Apply(m_After); }
            void Revert() override { Apply(m_Before); }
            Entity GetFocusEntity() const override { return Resolve(); }

        private:
            Entity Resolve() const { return m_Scene ? m_Scene->TryGetEntityFromUuid(m_Target) : Entity{}; }

            void Apply(const glm::mat4& transform)
            {
                if (Entity target = Resolve())
                    target.SetWorldTransform(transform);
            }

            Ref<Scene> m_Scene;
            UUID m_Target = UUID::EMPTY;
            glm::mat4 m_Before{ 1.0f };
            glm::mat4 m_After{ 1.0f };
        };
    } // namespace

    ViewportTransformInteraction::~ViewportTransformInteraction() { Cancel(); }

    bool ViewportTransformInteraction::Begin(const Vector<Entity>& targets, const glm::mat4& pivot)
    {
        if (IsActive())
            return false;

        m_Snapshots.clear();
        m_Snapshots.reserve(targets.size());
        for (Entity target : targets)
        {
            if (!target)
                continue;
            m_Snapshots.push_back({ Ref<Scene>(target.GetScene()), target.GetUuid(), target.GetWorldMatrix() });
        }
        if (m_Snapshots.empty())
            return false;

        m_InitialPivot = pivot;
        m_CurrentPivot = pivot;
        if (!m_Transaction.Begin(TransactionId, [this] { return BuildAction(); }))
        {
            Reset();
            return false;
        }
        return true;
    }

    bool ViewportTransformInteraction::Update(const glm::mat4& pivot)
    {
        if (!IsActive())
            return false;

        m_CurrentPivot = pivot;
        const glm::mat4 delta = pivot * glm::inverse(m_InitialPivot);
        for (const Snapshot& snapshot : m_Snapshots)
        {
            if (Entity target = ResolveTarget(snapshot))
                target.SetWorldTransform(delta * snapshot.WorldTransform);
        }
        m_Transaction.Update(TransactionId, MatrixChanged(m_InitialPivot, m_CurrentPivot));
        return true;
    }

    ViewportTransformResolution ViewportTransformInteraction::Resolve(bool gizmoUsing, bool cancelRequested)
    {
        const TransformInteractionCompletion completion = ResolveTransformInteractionCompletion(IsActive(), gizmoUsing, cancelRequested);
        if (completion == TransformInteractionCompletion::Cancel)
        {
            Cancel();
            return { completion, {} };
        }
        if (completion == TransformInteractionCompletion::Commit)
            return { completion, Commit() };
        return { completion, {} };
    }

    void ViewportTransformInteraction::Cancel()
    {
        if (!IsActive())
            return;

        for (const Snapshot& snapshot : m_Snapshots)
        {
            if (Entity target = ResolveTarget(snapshot))
                target.SetWorldTransform(snapshot.WorldTransform);
        }
        m_Transaction.Cancel(TransactionId);
        Reset();
    }

    bool ViewportTransformInteraction::MatrixChanged(const glm::mat4& lhs, const glm::mat4& rhs)
    {
        for (uint32_t column = 0; column < 4u; ++column)
        {
            if (glm::any(glm::greaterThan(glm::abs(lhs[column] - rhs[column]), glm::vec4(TransformEpsilon))))
                return true;
        }
        return false;
    }

    Entity ViewportTransformInteraction::ResolveTarget(const Snapshot& snapshot)
    {
        return snapshot.SceneRef ? snapshot.SceneRef->TryGetEntityFromUuid(snapshot.Target) : Entity{};
    }

    Ref<UndoAction> ViewportTransformInteraction::Commit()
    {
        Ref<UndoAction> action = m_Transaction.Commit(TransactionId);
        Reset();
        return action;
    }

    Ref<UndoAction> ViewportTransformInteraction::BuildAction() const
    {
        Ref<UndoActionGroup> actions = CreateRef<UndoActionGroup>(m_Snapshots.size() == 1u ? "Transform entity" : "Transform entities");
        for (const Snapshot& snapshot : m_Snapshots)
        {
            Entity target = ResolveTarget(snapshot);
            if (!target)
                continue;
            const glm::mat4 current = target.GetWorldMatrix();
            if (MatrixChanged(snapshot.WorldTransform, current))
                actions->Add(CreateRef<WorldTransformAction>(target, snapshot.WorldTransform, current));
        }
        return actions->Empty() ? Ref<UndoAction>{} : actions;
    }

    void ViewportTransformInteraction::Reset()
    {
        m_Snapshots.clear();
        m_InitialPivot = glm::mat4(1.0f);
        m_CurrentPivot = glm::mat4(1.0f);
    }
} // namespace Crowny
