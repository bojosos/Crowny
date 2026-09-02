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
                    auto& cached = m_Snapshots[snapshotIndex];
                    cached.Target = entity;
                    cached.Component = entity.template GetComponent<T>();
                    cached.Prefab = CapturePrefab(entity);
                }
                else
                    m_Snapshots.push_back({ entity, entity.template GetComponent<T>(), CapturePrefab(entity) });
                snapshotIndex++;
            }

            if (snapshotIndex < m_Snapshots.size())
                m_Snapshots.erase(m_Snapshots.begin() + static_cast<std::ptrdiff_t>(snapshotIndex), m_Snapshots.end());
        }

        Ref<UndoAction> Build() const override
        {
            if (m_PendingAction != nullptr)
                return {};

            m_PendingAction = CreateRef<ChangeComponentsAction<T>>(m_Snapshots);
            return m_PendingAction;
        }

        void CompleteFrame()
        {
            if (m_PendingAction == nullptr)
                return;

            m_PendingAction->FinalizeNewValues();
            m_PendingAction = nullptr;
        }

        void Reset() override
        {
            m_Snapshots.clear();
            m_PendingAction = nullptr;
        }

        size_t Size() const { return m_Snapshots.size(); }
        size_t Capacity() const { return m_Snapshots.capacity(); }

    private:
        static std::optional<PrefabComponent> CapturePrefab(Entity entity)
        {
            if (!entity.HasComponent<PrefabComponent>())
                return std::nullopt;
            return entity.GetComponent<PrefabComponent>();
        }

        Vector<typename ChangeComponentsAction<T>::BeforeValue> m_Snapshots;
        mutable Ref<ChangeComponentsAction<T>> m_PendingAction;
    };
} // namespace Crowny
