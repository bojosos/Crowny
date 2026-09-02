#include "cwepch.h"

#include "Editor/ViewportTransformInteraction.h"

#include "Crowny/Scene/Scene.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace Crowny
{
    namespace
    {
        constexpr float TransformEpsilon = 0.00001f;
        constexpr StringView PositionOverride = "Transform.Position";
        constexpr StringView RotationOverride = "Transform.Rotation";
        constexpr StringView ScaleOverride = "Transform.Scale";

        struct TransformOverrideState
        {
            bool Position = false;
            bool Rotation = false;
            bool Scale = false;
        };

        bool VectorChanged(const glm::vec3& lhs, const glm::vec3& rhs)
        {
            return glm::any(glm::greaterThan(glm::abs(lhs - rhs), glm::vec3(TransformEpsilon)));
        }

        bool RotationChanged(const glm::quat& lhs, const glm::quat& rhs)
        {
            const glm::quat left = glm::normalize(lhs);
            glm::quat right = glm::normalize(rhs);
            if (glm::dot(left, right) < 0.0f)
                right = -right;
            return glm::any(glm::greaterThan(glm::abs(glm::vec4(left.x, left.y, left.z, left.w) - glm::vec4(right.x, right.y, right.z, right.w)),
                                             glm::vec4(TransformEpsilon)));
        }

        bool IsFinite(const glm::vec3& value) { return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z); }

        bool IsFinite(const glm::quat& value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) && std::isfinite(value.w);
        }

        glm::vec3 ScaleFactorsForOrientation(const glm::vec3& frameScale, const glm::quat& frameRotation, const glm::quat& targetRotation)
        {
            const glm::mat3 targetInFrame = glm::mat3_cast(glm::inverse(frameRotation) * targetRotation);
            return { glm::length(frameScale * targetInFrame[0]), glm::length(frameScale * targetInFrame[1]),
                     glm::length(frameScale * targetInFrame[2]) };
        }

        TransformOverrideState CaptureOverrides(Entity entity)
        {
            if (!entity || !entity.HasComponent<PrefabComponent>())
                return {};
            const PrefabComponent& prefab = entity.GetComponent<PrefabComponent>();
            return { prefab.IsPropertyOverridden(PositionOverride), prefab.IsPropertyOverridden(RotationOverride),
                     prefab.IsPropertyOverridden(ScaleOverride) };
        }

        void SetOverride(PrefabComponent& prefab, StringView path, bool overridden)
        {
            if (overridden)
                prefab.MarkOverridden(String(path));
            else
                prefab.ClearOverride(path);
        }

        void ApplyOverrides(Entity entity, const TransformOverrideState& state)
        {
            if (!entity || !entity.HasComponent<PrefabComponent>())
                return;
            PrefabComponent& prefab = entity.GetComponent<PrefabComponent>();
            SetOverride(prefab, PositionOverride, state.Position);
            SetOverride(prefab, RotationOverride, state.Rotation);
            SetOverride(prefab, ScaleOverride, state.Scale);
        }

        void ApplyOverride(Entity entity, StringView path, bool initiallyOverridden, bool changed)
        {
            if (entity && entity.HasComponent<PrefabComponent>())
                SetOverride(entity.GetComponent<PrefabComponent>(), path, initiallyOverridden || changed);
        }

        class WorldTransformAction final : public UndoAction
        {
        public:
            WorldTransformAction(Entity target, const glm::mat4& before, const glm::mat4& after, TransformOverrideState beforeOverrides,
                                 TransformOverrideState afterOverrides)
              : UndoAction("Transform entity"), m_Scene(target.GetScene()), m_Target(target.GetUuid()), m_Before(before), m_After(after),
                m_BeforeOverrides(beforeOverrides), m_AfterOverrides(afterOverrides)
            {
            }

            void Commit() override { Apply(m_After, m_AfterOverrides); }
            void Revert() override { Apply(m_Before, m_BeforeOverrides); }
            Entity GetFocusEntity() const override { return Resolve(); }

        private:
            Entity Resolve() const { return m_Scene ? m_Scene->TryGetEntityFromUuid(m_Target) : Entity{}; }

            void Apply(const glm::mat4& transform, const TransformOverrideState& overrides)
            {
                if (Entity target = Resolve(); target && target.SetWorldTransform(transform))
                    ApplyOverrides(target, overrides);
            }

            Ref<Scene> m_Scene;
            UUID m_Target = UUID::EMPTY;
            glm::mat4 m_Before{ 1.0f };
            glm::mat4 m_After{ 1.0f };
            TransformOverrideState m_BeforeOverrides;
            TransformOverrideState m_AfterOverrides;
        };
    } // namespace

    ViewportTransformInteraction::~ViewportTransformInteraction() { Cancel(); }

    Vector<Entity> ViewportTransformInteraction::NormalizeTargets(const Vector<Entity>& targets)
    {
        Vector<Entity> candidates;
        candidates.reserve(targets.size());
        for (Entity target : targets)
        {
            if (target && std::find(candidates.begin(), candidates.end(), target) == candidates.end())
                candidates.push_back(target);
        }

        Vector<Entity> roots;
        roots.reserve(candidates.size());
        for (Entity target : candidates)
        {
            bool selectedAncestor = false;
            Vector<Entity> visited;
            for (Entity parent = target.GetParent(); parent; parent = parent.GetParent())
            {
                if (std::find(visited.begin(), visited.end(), parent) != visited.end())
                    break;
                visited.push_back(parent);
                if (std::find(candidates.begin(), candidates.end(), parent) != candidates.end())
                {
                    selectedAncestor = true;
                    break;
                }
            }
            if (!selectedAncestor)
                roots.push_back(target);
        }
        return roots;
    }

    glm::mat4 ViewportTransformInteraction::CalculatePivot(const Vector<Entity>& targets, Entity primary, SelectionTransformSpace space)
    {
        const Vector<Entity> roots = NormalizeTargets(targets);
        if (roots.empty())
            return glm::mat4(1.0f);

        glm::vec3 minimum(FLT_MAX);
        glm::vec3 maximum(-FLT_MAX);
        for (Entity target : roots)
        {
            const glm::vec3 position = target.GetWorldPosition();
            minimum = glm::min(minimum, position);
            maximum = glm::max(maximum, position);
        }

        Entity orientationTarget;
        if (primary)
        {
            for (Entity root : roots)
            {
                if (primary == root || primary.IsChildOf(root))
                {
                    orientationTarget = root;
                    break;
                }
            }
        }
        if (!orientationTarget)
            orientationTarget = roots.front();

        const glm::vec3 center = (minimum + maximum) * 0.5f;
        const glm::quat orientation =
          space == SelectionTransformSpace::Local ? orientationTarget.GetWorldRotation() : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        return Math::ComposeMatrix(center, orientation, glm::vec3(1.0f));
    }

    bool ViewportTransformInteraction::Begin(const Vector<Entity>& targets, Entity primary, SelectionTransformOperation operation,
                                             SelectionTransformSpace space)
    {
        if (IsActive())
            return false;

        const Vector<Entity> roots = NormalizeTargets(targets);
        m_Snapshots.clear();
        m_Snapshots.reserve(roots.size());
        for (Entity target : roots)
        {
            const TransformOverrideState overrides = CaptureOverrides(target);
            m_Snapshots.push_back({ Ref<Scene>(target.GetScene()), target.GetUuid(), target.GetWorldMatrix(), target.GetWorldPosition(),
                                    target.GetWorldRotation(), target.GetWorldScale(), overrides.Position, overrides.Rotation, overrides.Scale });
        }
        if (m_Snapshots.empty())
            return false;

        m_Operation = operation;
        m_InitialPivot = CalculatePivot(roots, primary, space);
        m_CurrentPivot = m_InitialPivot;
        glm::vec3 ignoredScale;
        if (!Math::DecomposeMatrix(m_InitialPivot, m_InitialPivotPosition, m_InitialPivotRotation, ignoredScale))
        {
            Reset();
            return false;
        }
        m_InitialPivotRotation = glm::normalize(m_InitialPivotRotation);

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

        glm::vec3 pivotPosition;
        glm::quat pivotRotation;
        glm::vec3 pivotScale;
        if (!Math::DecomposeMatrix(pivot, pivotPosition, pivotRotation, pivotScale) || !IsFinite(pivotPosition) || !IsFinite(pivotRotation) ||
            !IsFinite(pivotScale))
            return false;

        pivotRotation = glm::normalize(pivotRotation);
        bool applied = false;
        glm::mat4 nextPivot = m_CurrentPivot;
        switch (m_Operation)
        {
        case SelectionTransformOperation::Translate:
            nextPivot = Math::ComposeMatrix(pivotPosition, m_InitialPivotRotation, glm::vec3(1.0f));
            applied = ApplyTranslation(pivotPosition);
            break;
        case SelectionTransformOperation::Rotate:
            nextPivot = Math::ComposeMatrix(pivotPosition, pivotRotation, glm::vec3(1.0f));
            applied = ApplyRotation(pivotPosition, pivotRotation);
            break;
        case SelectionTransformOperation::Scale:
            if (glm::any(glm::lessThanEqual(pivotScale, glm::vec3(0.001f))))
                return false;
            nextPivot = Math::ComposeMatrix(pivotPosition, m_InitialPivotRotation, pivotScale);
            applied = ApplyScale(pivotPosition, pivotScale);
            break;
        }

        if (applied)
            m_CurrentPivot = nextPivot;
        m_Transaction.Update(TransactionId, applied && MatrixChanged(m_InitialPivot, m_CurrentPivot));
        return applied;
    }

    ViewportTransformFrameResult ViewportTransformInteraction::ProcessGizmoFrame(
      const Vector<Entity>& targets, Entity primary, SelectionTransformOperation operation, SelectionTransformSpace space,
      const ViewportGizmoFrame& frame)
    {
        ViewportTransformFrameResult result;
        result.Manipulated = frame.Manipulated;
        result.GizmoUsing = frame.Using;
        result.CancelRequested = frame.CancelRequested;
        result.Before = Observe(primary);
        if ((frame.Using || frame.Manipulated) && !IsActive() && !m_BlockUntilRelease)
        {
            result.BeginAttempted = true;
            result.Began = Begin(targets, primary, operation, space);
        }
        if (frame.Manipulated)
        {
            result.UpdateAttempted = IsActive();
            result.Updated = result.UpdateAttempted && Update(frame.Pivot);
        }

        result.Resolution = Resolve(frame.Using, frame.CancelRequested);
        if (result.Resolution.State == TransformInteractionCompletion::Cancel && frame.Using)
            m_BlockUntilRelease = true;
        if (!frame.Using)
            m_BlockUntilRelease = false;
        result.BlockedUntilRelease = m_BlockUntilRelease;
        result.After = Observe(primary);
        return result;
    }

    bool ViewportTransformInteraction::ApplyTranslation(const glm::vec3& pivotPosition)
    {
        const glm::vec3 delta = pivotPosition - m_InitialPivotPosition;
        bool applied = false;
        for (const Snapshot& snapshot : m_Snapshots)
        {
            Entity target = ResolveTarget(snapshot);
            if (!target)
                continue;
            const glm::vec3 position = snapshot.WorldPosition + delta;
            const glm::mat4 transform = Math::ComposeMatrix(position, snapshot.WorldRotation, snapshot.WorldScale);
            if (MatrixChanged(target.GetWorldMatrix(), transform) && !target.SetWorldTransform(transform))
                continue;
            ApplyOverride(target, PositionOverride, snapshot.PositionOverridden, VectorChanged(snapshot.WorldPosition, position));
            applied = true;
        }
        return applied;
    }

    bool ViewportTransformInteraction::ApplyRotation(const glm::vec3& pivotPosition, const glm::quat& pivotRotation)
    {
        const glm::quat delta = glm::normalize(pivotRotation * glm::inverse(m_InitialPivotRotation));
        bool applied = false;
        for (const Snapshot& snapshot : m_Snapshots)
        {
            Entity target = ResolveTarget(snapshot);
            if (!target)
                continue;
            const glm::vec3 position = pivotPosition + delta * (snapshot.WorldPosition - m_InitialPivotPosition);
            const glm::quat rotation = glm::normalize(delta * snapshot.WorldRotation);
            const bool positionChanged = VectorChanged(snapshot.WorldPosition, position);
            const bool rotationChanged = RotationChanged(snapshot.WorldRotation, rotation);
            const glm::mat4 transform = Math::ComposeMatrix(position, rotation, snapshot.WorldScale);
            if (MatrixChanged(target.GetWorldMatrix(), transform) && !target.SetWorldTransform(transform))
                continue;
            ApplyOverride(target, PositionOverride, snapshot.PositionOverridden, positionChanged);
            ApplyOverride(target, RotationOverride, snapshot.RotationOverridden, rotationChanged);
            applied = true;
        }
        return applied;
    }

    bool ViewportTransformInteraction::ApplyScale(const glm::vec3& pivotPosition, const glm::vec3& pivotScale)
    {
        const glm::quat inverseBasis = glm::inverse(m_InitialPivotRotation);
        bool applied = false;
        for (const Snapshot& snapshot : m_Snapshots)
        {
            Entity target = ResolveTarget(snapshot);
            if (!target)
                continue;

            const glm::vec3 localOffset = inverseBasis * (snapshot.WorldPosition - m_InitialPivotPosition);
            const glm::vec3 position = pivotPosition + m_InitialPivotRotation * (localOffset * pivotScale);
            const glm::vec3 scale = snapshot.WorldScale * ScaleFactorsForOrientation(pivotScale, m_InitialPivotRotation, snapshot.WorldRotation);
            const bool positionChanged = VectorChanged(snapshot.WorldPosition, position);
            const bool scaleChanged = VectorChanged(snapshot.WorldScale, scale);
            const glm::mat4 transform = Math::ComposeMatrix(position, snapshot.WorldRotation, scale);
            if (MatrixChanged(target.GetWorldMatrix(), transform) && !target.SetWorldTransform(transform))
                continue;
            ApplyOverride(target, PositionOverride, snapshot.PositionOverridden, positionChanged);
            ApplyOverride(target, ScaleOverride, snapshot.ScaleOverridden, scaleChanged);
            applied = true;
        }
        return applied;
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
            if (Entity target = ResolveTarget(snapshot); target && target.SetWorldTransform(snapshot.WorldTransform))
                ApplyOverrides(target, { snapshot.PositionOverridden, snapshot.RotationOverridden, snapshot.ScaleOverridden });
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

    ViewportTransformObservedState ViewportTransformInteraction::Observe(Entity entity)
    {
        if (!entity)
            return {};
        return { true, entity.GetWorldPosition(), entity.GetLocalPosition() };
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
            if (!MatrixChanged(snapshot.WorldTransform, current))
                continue;
            actions->Add(CreateRef<WorldTransformAction>(
              target, snapshot.WorldTransform, current,
              TransformOverrideState{ snapshot.PositionOverridden, snapshot.RotationOverridden, snapshot.ScaleOverridden },
              CaptureOverrides(target)));
        }
        return actions->Empty() ? Ref<UndoAction>{} : actions;
    }

    void ViewportTransformInteraction::Reset()
    {
        m_Snapshots.clear();
        m_Operation = SelectionTransformOperation::Translate;
        m_InitialPivot = glm::mat4(1.0f);
        m_CurrentPivot = glm::mat4(1.0f);
        m_InitialPivotPosition = glm::vec3(0.0f);
        m_InitialPivotRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        m_BlockUntilRelease = false;
    }
} // namespace Crowny
