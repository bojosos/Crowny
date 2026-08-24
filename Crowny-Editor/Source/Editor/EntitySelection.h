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

    class EntitySelection
    {
    public:
        bool Select(Entity entity, EntitySelectionMode mode, const Vector<Entity>& orderedEntities = {})
        {
            if (!entity)
                return Clear();
            if ((mode == EntitySelectionMode::Range || mode == EntitySelectionMode::AddRange) && m_Anchor)
            {
                const auto anchor = std::find(orderedEntities.begin(), orderedEntities.end(), m_Anchor);
                const auto target = std::find(orderedEntities.begin(), orderedEntities.end(), entity);
                if (anchor != orderedEntities.end() && target != orderedEntities.end())
                {
                    const auto first = std::min(anchor, target);
                    const auto last = std::max(anchor, target);
                    Vector<Entity> next = mode == EntitySelectionMode::AddRange ? m_Selected : Vector<Entity>{};
                    for (auto current = first; current != last + 1; ++current)
                    {
                        if (std::find(next.begin(), next.end(), *current) == next.end())
                            next.push_back(*current);
                    }
                    const bool changed = next != m_Selected || m_Primary != entity;
                    m_Selected = std::move(next);
                    m_Primary = entity;
                    return changed;
                }
                mode = EntitySelectionMode::Replace;
            }

            if (mode == EntitySelectionMode::Add)
            {
                if (Contains(entity))
                {
                    const bool changed = m_Primary != entity;
                    m_Primary = entity;
                    return changed;
                }
                m_Selected.push_back(entity);
                m_Primary = entity;
                m_Anchor = entity;
                return true;
            }

            if (mode == EntitySelectionMode::Toggle)
            {
                const auto selected = std::find(m_Selected.begin(), m_Selected.end(), entity);
                if (selected == m_Selected.end())
                {
                    m_Selected.push_back(entity);
                    m_Primary = entity;
                    m_Anchor = entity;
                }
                else
                {
                    m_Selected.erase(selected);
                    m_Primary = m_Selected.empty() ? Entity{} : m_Selected.back();
                    if (m_Anchor == entity)
                        m_Anchor = m_Primary;
                }
                return true;
            }

            if (m_Selected.size() == 1 && m_Selected.front() == entity && m_Primary == entity)
                return false;
            m_Selected = { entity };
            m_Primary = entity;
            m_Anchor = entity;
            return true;
        }

        bool Clear()
        {
            if (m_Selected.empty() && !m_Primary && !m_Anchor)
                return false;
            m_Selected.clear();
            m_Primary = {};
            m_Anchor = {};
            return true;
        }

        bool Prune(Scene* scene)
        {
            const size_t oldSize = m_Selected.size();
            const Entity oldPrimary = m_Primary;
            const Entity oldAnchor = m_Anchor;
            m_Selected.erase(std::remove_if(m_Selected.begin(), m_Selected.end(),
                                            [scene](Entity entity) { return !entity.IsValid() || entity.GetScene() != scene; }),
                             m_Selected.end());
            if (!m_Primary.IsValid() || m_Primary.GetScene() != scene || !Contains(m_Primary))
                m_Primary = m_Selected.empty() ? Entity{} : m_Selected.back();
            if (!m_Anchor.IsValid() || m_Anchor.GetScene() != scene || !Contains(m_Anchor))
                m_Anchor = m_Primary;
            return oldSize != m_Selected.size() || oldPrimary != m_Primary || oldAnchor != m_Anchor;
        }

        bool Contains(Entity entity) const { return std::find(m_Selected.begin(), m_Selected.end(), entity) != m_Selected.end(); }

        Entity GetPrimary() const { return m_Primary; }
        const Vector<Entity>& GetAll() const { return m_Selected; }
        bool Empty() const { return m_Selected.empty(); }

    private:
        Vector<Entity> m_Selected;
        Entity m_Primary;
        Entity m_Anchor;
    };
} // namespace Crowny
