#pragma once

#include "Crowny/Ecs/Entity.h"

namespace Crowny
{
    enum class EntitySelectionMode
    {
        Replace,
        Add,
        Toggle,
        Range,
        AddRange
    };

    inline EntitySelectionMode ResolveEntitySelectionMode(bool ctrl, bool shift)
    {
        if (shift)
            return ctrl ? EntitySelectionMode::AddRange : EntitySelectionMode::Range;
        return ctrl ? EntitySelectionMode::Toggle : EntitySelectionMode::Replace;
    }

    class EntitySelection
    {
    public:
        // orderedEntities is the hierarchy's current visible order and contains each entity once.
        bool Select(Entity entity, EntitySelectionMode mode, const Vector<Entity>& orderedEntities = {})
        {
            if (!entity)
                return Clear();

            bool changed = Prune(entity.GetScene());
            if (mode == EntitySelectionMode::Range || mode == EntitySelectionMode::AddRange)
            {
                const auto target = std::find(orderedEntities.begin(), orderedEntities.end(), entity);
                const auto anchor = FindRangeAnchor(orderedEntities);
                if (anchor != orderedEntities.end() && target != orderedEntities.end())
                {
                    const auto first = std::min(anchor, target);
                    const auto last = std::max(anchor, target);
                    m_RangeScratch.clear();
                    m_RangeScratch.reserve((mode == EntitySelectionMode::AddRange ? m_Selected.size() : 0u) +
                                           static_cast<size_t>(std::distance(first, last)) + 1u);
                    if (mode == EntitySelectionMode::AddRange)
                        m_RangeScratch.insert(m_RangeScratch.end(), m_Selected.begin(), m_Selected.end());

                    for (auto current = first;; ++current)
                    {
                        if (IsSelectable(*current, entity.GetScene()) && (mode != EntitySelectionMode::AddRange || !Contains(*current)))
                            m_RangeScratch.push_back(*current);
                        if (current == last)
                            break;
                    }

                    const bool selectionChanged = m_RangeScratch != m_Selected;
                    changed = changed || selectionChanged || m_Primary != entity || m_Anchor != *anchor;
                    m_Selected.swap(m_RangeScratch);
                    m_RangeScratch.clear();
                    m_Primary = entity;
                    m_Anchor = *anchor;
                    if (selectionChanged)
                        RebuildMembership();
                    return changed;
                }
                mode = EntitySelectionMode::Replace;
            }

            if (mode == EntitySelectionMode::Add)
            {
                if (Contains(entity))
                {
                    changed = changed || m_Primary != entity;
                    m_Primary = entity;
                    return changed;
                }
                m_Selected.push_back(entity);
                m_Primary = entity;
                m_Anchor = entity;
                RebuildMembership();
                return true;
            }

            if (mode == EntitySelectionMode::Toggle)
            {
                const auto selected = std::find(m_Selected.begin(), m_Selected.end(), entity);
                m_Anchor = entity;
                if (selected == m_Selected.end())
                {
                    m_Selected.push_back(entity);
                    m_Primary = entity;
                }
                else
                {
                    const bool removedPrimary = m_Primary == entity;
                    m_Selected.erase(selected);
                    if (m_Selected.empty())
                        m_Primary = {};
                    else if (removedPrimary || std::find(m_Selected.begin(), m_Selected.end(), m_Primary) == m_Selected.end())
                        m_Primary = m_Selected.back();
                }
                RebuildMembership();
                return true;
            }

            const bool selectionChanged = m_Selected.size() != 1u || m_Selected.front() != entity;
            changed = changed || selectionChanged || m_Primary != entity || m_Anchor != entity;
            m_Selected.clear();
            m_Selected.push_back(entity);
            m_Primary = entity;
            m_Anchor = entity;
            if (selectionChanged)
                RebuildMembership();
            return changed;
        }

        bool Clear()
        {
            if (m_Selected.empty() && m_Primary.GetScene() == nullptr && m_Anchor.GetScene() == nullptr)
                return false;
            m_Selected.clear();
            m_Primary = {};
            m_Anchor = {};
            m_RangeScratch.clear();
            m_Membership.clear();
            m_MembershipScene = nullptr;
            return true;
        }

        bool Prune(Scene* scene)
        {
            const size_t oldSize = m_Selected.size();
            const Entity oldPrimary = m_Primary;
            const Entity oldAnchor = m_Anchor;
            m_Selected.erase(std::remove_if(m_Selected.begin(), m_Selected.end(), [scene](Entity entity) { return !IsSelectable(entity, scene); }),
                             m_Selected.end());
            if (!IsSelectable(m_Primary, scene) || std::find(m_Selected.begin(), m_Selected.end(), m_Primary) == m_Selected.end())
                m_Primary = m_Selected.empty() ? Entity{} : m_Selected.back();
            if (!IsSelectable(m_Anchor, scene))
                m_Anchor = m_Primary;
            const bool selectionChanged = oldSize != m_Selected.size();
            if (selectionChanged)
                RebuildMembership();
            return selectionChanged || oldPrimary != m_Primary || oldAnchor != m_Anchor;
        }

        bool Contains(Entity entity) const
        {
            if (entity.GetScene() != m_MembershipScene)
                return false;
            return std::binary_search(m_Membership.begin(), m_Membership.end(), entity.GetHandle(), HandleLess);
        }

        Entity GetPrimary() const { return m_Primary; }
        const Vector<Entity>& GetAll() const { return m_Selected; }
        bool Empty() const { return m_Selected.empty(); }

    private:
        static bool IsSelectable(Entity entity, Scene* scene) { return scene != nullptr && entity.GetScene() == scene && entity.IsValid(); }

        static bool HandleLess(entt::entity lhs, entt::entity rhs) { return entt::to_integral(lhs) < entt::to_integral(rhs); }

        void RebuildMembership()
        {
            m_Membership.clear();
            m_MembershipScene = m_Selected.empty() ? nullptr : m_Selected.front().GetScene();
            m_Membership.reserve(m_Selected.size());
            for (Entity entity : m_Selected)
                m_Membership.push_back(entity.GetHandle());
            std::sort(m_Membership.begin(), m_Membership.end(), HandleLess);
        }

        Vector<Entity>::const_iterator FindRangeAnchor(const Vector<Entity>& orderedEntities) const
        {
            const auto end = orderedEntities.end();
            if (m_Anchor)
            {
                const auto anchor = std::find(orderedEntities.begin(), end, m_Anchor);
                if (anchor != end)
                    return anchor;
            }
            if (m_Primary)
            {
                const auto primary = std::find(orderedEntities.begin(), end, m_Primary);
                if (primary != end)
                    return primary;
            }
            for (auto selected = m_Selected.rbegin(); selected != m_Selected.rend(); ++selected)
            {
                const auto visible = std::find(orderedEntities.begin(), end, *selected);
                if (visible != end)
                    return visible;
            }
            return end;
        }

        Vector<Entity> m_Selected;
        Vector<Entity> m_RangeScratch;
        Vector<entt::entity> m_Membership;
        Scene* m_MembershipScene = nullptr;
        Entity m_Primary;
        Entity m_Anchor;
    };
} // namespace Crowny
