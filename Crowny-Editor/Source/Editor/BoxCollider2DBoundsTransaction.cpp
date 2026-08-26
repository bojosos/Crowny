#include "cwepch.h"

#include "Editor/BoxCollider2DBoundsTransaction.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Scene/Scene.h"

#include <cmath>

namespace Crowny
{
    namespace
    {
        constexpr float BoundsEpsilon = 0.00001f;

        bool BoundsChanged(const glm::vec2& lhsOffset, const glm::vec2& lhsSize, const glm::vec2& rhsOffset, const glm::vec2& rhsSize)
        {
            return glm::any(glm::greaterThan(glm::abs(lhsOffset - rhsOffset), glm::vec2(BoundsEpsilon))) ||
                   glm::any(glm::greaterThan(glm::abs(lhsSize - rhsSize), glm::vec2(BoundsEpsilon)));
        }

        void ApplyBounds(Entity target, const glm::vec2& offset, const glm::vec2& size)
        {
            if (!target || !target.HasComponent<BoxCollider2DComponent>())
                return;

            BoxCollider2DComponent updated = target.GetComponent<BoxCollider2DComponent>();
            updated.SetOffset(offset, {});
            updated.SetSize(size, {});
            target.AddOrReplaceComponent<BoxCollider2DComponent>(updated);
        }

        class BoxCollider2DBoundsAction final : public UndoAction
        {
        public:
            BoxCollider2DBoundsAction(Entity target, const glm::vec2& beforeOffset, const glm::vec2& beforeSize, const glm::vec2& afterOffset,
                                      const glm::vec2& afterSize)
              : UndoAction("Edit Box Collider 2D bounds"), m_Scene(target.GetScene()), m_Target(target.GetUuid()), m_BeforeOffset(beforeOffset),
                m_BeforeSize(beforeSize), m_AfterOffset(afterOffset), m_AfterSize(afterSize)
            {
            }

            void Commit() override { ApplyBounds(Resolve(), m_AfterOffset, m_AfterSize); }
            void Revert() override { ApplyBounds(Resolve(), m_BeforeOffset, m_BeforeSize); }
            Entity GetFocusEntity() const override { return Resolve(); }

        private:
            Entity Resolve() const { return m_Scene ? m_Scene->TryGetEntityFromUuid(m_Target) : Entity{}; }

            Ref<Scene> m_Scene;
            UUID m_Target = UUID::EMPTY;
            glm::vec2 m_BeforeOffset{ 0.0f };
            glm::vec2 m_BeforeSize{ 0.5f };
            glm::vec2 m_AfterOffset{ 0.0f };
            glm::vec2 m_AfterSize{ 0.5f };
        };
    } // namespace

    bool BoxCollider2DBoundsTransaction::Begin(Entity target)
    {
        if (IsActive() || !target || !target.HasComponent<BoxCollider2DComponent>())
            return false;

        m_Scene = target.GetScene();
        m_Target = target.GetUuid();
        m_Before = Capture(target);
        if (!m_Transaction.Begin(TransactionId, [this] { return BuildAction(); }))
        {
            ResetTarget();
            return false;
        }
        return true;
    }

    bool BoxCollider2DBoundsTransaction::Update(const glm::vec2& offset, const glm::vec2& size)
    {
        if (!IsActive())
            return false;

        const Bounds bounds{ offset, size };
        if (!IsValid(bounds))
            return false;

        Entity target = ResolveTarget();
        if (!target || !target.HasComponent<BoxCollider2DComponent>())
        {
            m_Transaction.Cancel(TransactionId);
            ResetTarget();
            return false;
        }

        const Bounds current = Capture(target);
        if (Changed(current, bounds))
            Apply(target, bounds);
        m_Transaction.Update(TransactionId, Changed(m_Before, Capture(target)));
        return true;
    }

    Ref<UndoAction> BoxCollider2DBoundsTransaction::Commit()
    {
        Ref<UndoAction> action = m_Transaction.Commit(TransactionId);
        ResetTarget();
        return action;
    }

    void BoxCollider2DBoundsTransaction::Cancel()
    {
        if (!IsActive())
            return;

        Entity target = ResolveTarget();
        if (target && target.HasComponent<BoxCollider2DComponent>() && Changed(m_Before, Capture(target)))
            Apply(target, m_Before);
        m_Transaction.Cancel(TransactionId);
        ResetTarget();
    }

    bool BoxCollider2DBoundsTransaction::Owns(Entity target) const
    {
        return IsActive() && target && m_Scene.get() == target.GetScene() && m_Target == target.GetUuid();
    }

    bool BoxCollider2DBoundsTransaction::IsValid(const Bounds& bounds)
    {
        return std::isfinite(bounds.Offset.x) && std::isfinite(bounds.Offset.y) && std::isfinite(bounds.Size.x) &&
               std::isfinite(bounds.Size.y) && bounds.Size.x > 0.0f && bounds.Size.y > 0.0f;
    }

    bool BoxCollider2DBoundsTransaction::Changed(const Bounds& lhs, const Bounds& rhs)
    {
        return BoundsChanged(lhs.Offset, lhs.Size, rhs.Offset, rhs.Size);
    }

    BoxCollider2DBoundsTransaction::Bounds BoxCollider2DBoundsTransaction::Capture(Entity target)
    {
        const BoxCollider2DComponent& collider = target.GetComponent<BoxCollider2DComponent>();
        return { collider.GetOffset(), collider.GetSize() };
    }

    void BoxCollider2DBoundsTransaction::Apply(Entity target, const Bounds& bounds)
    {
        ApplyBounds(target, bounds.Offset, bounds.Size);
    }

    Entity BoxCollider2DBoundsTransaction::ResolveTarget() const
    {
        return m_Scene ? m_Scene->TryGetEntityFromUuid(m_Target) : Entity{};
    }

    Ref<UndoAction> BoxCollider2DBoundsTransaction::BuildAction() const
    {
        Entity target = ResolveTarget();
        if (!target || !target.HasComponent<BoxCollider2DComponent>())
            return {};

        const Bounds after = Capture(target);
        if (!Changed(m_Before, after))
            return {};
        return CreateRef<BoxCollider2DBoundsAction>(target, m_Before.Offset, m_Before.Size, after.Offset, after.Size);
    }

    void BoxCollider2DBoundsTransaction::ResetTarget()
    {
        m_Scene = nullptr;
        m_Target = UUID::EMPTY;
        m_Before = {};
    }
} // namespace Crowny
