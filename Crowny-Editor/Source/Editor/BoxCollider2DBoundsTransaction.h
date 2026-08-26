#pragma once

#include "Editor/UndoRedo.h"

namespace Crowny
{
    class BoxCollider2DBoundsTransaction
    {
    public:
        ~BoxCollider2DBoundsTransaction();

        bool Begin(Entity target);
        bool Update(const glm::vec2& offset, const glm::vec2& size);
        Ref<UndoAction> Commit();
        void Cancel();

        bool IsActive() const { return m_Transaction.IsActive(); }
        bool Owns(Entity target) const;

    private:
        struct Bounds
        {
            glm::vec2 Offset{ 0.0f };
            glm::vec2 Size{ 0.5f };
        };

        static bool IsValid(const Bounds& bounds);
        static bool Changed(const Bounds& lhs, const Bounds& rhs);
        static Bounds Capture(Entity target);
        static void Apply(Entity target, const Bounds& bounds);

        Entity ResolveTarget() const;
        Ref<UndoAction> BuildAction() const;
        void ResetTarget();

        static constexpr UndoTransaction::Id TransactionId = 1u;
        UndoTransaction m_Transaction;
        Ref<Scene> m_Scene;
        UUID m_Target = UUID::EMPTY;
        Bounds m_Before;
    };
} // namespace Crowny
