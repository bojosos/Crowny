#include "cwpch.h"

#include "Crowny/Renderer/DecalWorld.h"

namespace Crowny
{
    DecalHandle DecalWorld::Create(const DecalRecord& record)
    {
        uint32_t index;
        if (m_Free.empty())
        {
            index = static_cast<uint32_t>(m_Slots.size());
            m_Slots.emplace_back();
        }
        else
        {
            index = m_Free.back();
            m_Free.pop_back();
        }
        Slot& slot = m_Slots[index];
        slot.Record = record;
        slot.Alive = true;
        DecalHandle handle{ index, slot.Generation };
        m_Changes.push_back({ handle, DecalChangeType::Create, record });
        return handle;
    }
    bool DecalWorld::IsAlive(DecalHandle handle) const
    {
        return handle.Index < m_Slots.size() && m_Slots[handle.Index].Alive && m_Slots[handle.Index].Generation == handle.Generation;
    }
    bool DecalWorld::Update(DecalHandle handle, const DecalRecord& record)
    {
        if (!IsAlive(handle))
            return false;
        m_Slots[handle.Index].Record = record;
        m_Changes.push_back({ handle, DecalChangeType::Update, record });
        return true;
    }
    bool DecalWorld::Destroy(DecalHandle handle)
    {
        if (!IsAlive(handle))
            return false;
        Slot& slot = m_Slots[handle.Index];
        slot.Alive = false;
        if (++slot.Generation == 0)
            ++slot.Generation;
        slot.Record = {};
        m_Free.push_back(handle.Index);
        m_Changes.push_back({ handle, DecalChangeType::Destroy, {} });
        return true;
    }
    bool DecalWorld::TryGet(DecalHandle handle, DecalRecord& record) const
    {
        if (!IsAlive(handle))
            return false;
        record = m_Slots[handle.Index].Record;
        return true;
    }
    void DecalWorld::DrainChanges(Vector<DecalChange>& changes)
    {
        changes.clear();
        changes.swap(m_Changes);
    }
    void DecalWorld::Clear()
    {
        for (uint32_t index = 0; index < m_Slots.size(); ++index)
            if (m_Slots[index].Alive)
                Destroy({ index, m_Slots[index].Generation });
    }
} // namespace Crowny
