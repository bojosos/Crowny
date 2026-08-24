#include "cwpch.h"

#include "Crowny/Renderer/BindlessResourceTable.h"

namespace Crowny
{
    BindlessResourceTable::BindlessResourceTable(uint32_t capacity, uint64_t fallbackResourceID)
      : m_FallbackResourceID(fallbackResourceID)
    {
        capacity = std::clamp(capacity, 1u, BindlessResourceHandle::MaxResources);
        m_Slots.resize(capacity);
        m_Slots[0].ResourceID = fallbackResourceID;
        m_Slots[0].Occupied = true;
        m_Slots[0].Dirty = true;
        m_FreeSlots.reserve(capacity - 1u);
        m_RetiredSlots.reserve(capacity - 1u);
        for (uint32_t index = capacity; index > 1; index--)
            m_FreeSlots.push_back(index - 1u);
    }

    BindlessResourceHandle BindlessResourceTable::Allocate(uint64_t resourceID)
    {
        ScopedLock lock(m_Mutex);
        if (m_FreeSlots.empty())
            return {};

        const uint32_t index = m_FreeSlots.back();
        m_FreeSlots.pop_back();
        Slot& slot = m_Slots[index];
        slot.ResourceID = resourceID != 0 ? resourceID : m_FallbackResourceID;
        slot.Occupied = true;
        slot.Dirty = true;
        m_ActiveCount++;
        return BindlessResourceHandle::FromParts(index, slot.Generation);
    }

    bool BindlessResourceTable::Replace(BindlessResourceHandle handle, uint64_t resourceID)
    {
        ScopedLock lock(m_Mutex);
        if (!Validate(handle))
            return false;
        Slot& slot = m_Slots[handle.GetIndex()];
        slot.ResourceID = resourceID != 0 ? resourceID : m_FallbackResourceID;
        slot.Dirty = true;
        return true;
    }

    bool BindlessResourceTable::Release(BindlessResourceHandle handle, uint64_t retireValue)
    {
        ScopedLock lock(m_Mutex);
        if (!Validate(handle) || handle.GetIndex() == 0)
            return false;

        Slot& slot = m_Slots[handle.GetIndex()];
        slot.ResourceID = m_FallbackResourceID;
        slot.Occupied = false;
        slot.Dirty = true;
        slot.Generation = NextGeneration(slot.Generation);
        m_RetiredSlots.push_back({ handle.GetIndex(), retireValue });
        m_ActiveCount--;
        return true;
    }

    void BindlessResourceTable::Collect(uint64_t completedValue)
    {
        ScopedLock lock(m_Mutex);
        for (auto retired = m_RetiredSlots.begin(); retired != m_RetiredSlots.end();)
        {
            if (retired->RetireValue > completedValue)
            {
                ++retired;
                continue;
            }
            m_FreeSlots.push_back(retired->Index);
            retired = m_RetiredSlots.erase(retired);
        }
    }

    uint64_t BindlessResourceTable::Resolve(BindlessResourceHandle handle) const
    {
        ScopedLock lock(m_Mutex);
        return Validate(handle) ? m_Slots[handle.GetIndex()].ResourceID : m_FallbackResourceID;
    }

    uint32_t BindlessResourceTable::GetDescriptorIndex(BindlessResourceHandle handle) const
    {
        ScopedLock lock(m_Mutex);
        return Validate(handle) ? handle.GetIndex() : 0u;
    }

    uint32_t BindlessResourceTable::GetActiveCount() const
    {
        ScopedLock lock(m_Mutex);
        return m_ActiveCount;
    }

    void BindlessResourceTable::DrainUpdates(Vector<BindlessResourceUpdate>& output)
    {
        ScopedLock lock(m_Mutex);
        output.clear();
        for (uint32_t index = 0; index < m_Slots.size(); index++)
        {
            Slot& slot = m_Slots[index];
            if (!slot.Dirty)
                continue;
            output.push_back({ index, slot.ResourceID });
            slot.Dirty = false;
        }
    }

    uint32_t BindlessResourceTable::NextGeneration(uint32_t generation)
    {
        generation = (generation + 1u) & BindlessResourceHandle::GenerationMask;
        return generation == 0 ? 1u : generation;
    }

    bool BindlessResourceTable::Validate(BindlessResourceHandle handle) const
    {
        if (!handle.IsValid() || handle.GetIndex() >= m_Slots.size())
            return false;
        const Slot& slot = m_Slots[handle.GetIndex()];
        return slot.Occupied && slot.Generation == handle.GetGeneration();
    }

} // namespace Crowny
