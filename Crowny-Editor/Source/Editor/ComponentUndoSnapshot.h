#pragma once

#include "Editor/UndoRedo.h"

#include <cstddef>
#include <type_traits>

namespace Crowny
{
    template <typename T> class ComponentUndoSnapshot final : public RetainedUndoActionFactory
    {
        static_assert(std::is_copy_constructible_v<T>, "Undo snapshots require copy-constructible components.");
        static_assert(std::is_copy_assignable_v<T>, "Retained undo snapshots require copy-assignable components.");

    public:
        void Capture(const Vector<Entity>& entities)
        {
            size_t snapshotIndex = 0u;
            for (Entity entity : entities)
            {
                if (!entity || !entity.template HasComponent<T>())
                    continue;

                if (snapshotIndex < m_Snapshots.size())
                {
                    auto& [cachedEntity, cachedComponent] = m_Snapshots[snapshotIndex];
                    cachedEntity = entity;
                    cachedComponent = entity.template GetComponent<T>();
                }
                else
                    m_Snapshots.emplace_back(entity, entity.template GetComponent<T>());
                snapshotIndex++;
            }

            if (snapshotIndex < m_Snapshots.size())
                m_Snapshots.erase(m_Snapshots.begin() + static_cast<std::ptrdiff_t>(snapshotIndex), m_Snapshots.end());
        }

        Ref<UndoAction> Build() const override { return CreateRef<ChangeComponentsAction<T>>(m_Snapshots); }
        void Reset() override { m_Snapshots.clear(); }

        size_t Size() const { return m_Snapshots.size(); }
        size_t Capacity() const { return m_Snapshots.capacity(); }

    private:
        Vector<Pair<Entity, T>> m_Snapshots;
    };
} // namespace Crowny
