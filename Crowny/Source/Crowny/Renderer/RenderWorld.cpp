#include "cwpch.h"

#include "Crowny/Renderer/RenderWorld.h"

#include <algorithm>
#include <cmath>

namespace Crowny
{
    AffineTransform3x4 AffineTransform3x4::FromMatrix(const glm::mat4& matrix)
    {
        AffineTransform3x4 result;
        result.Row0 = { matrix[0][0], matrix[1][0], matrix[2][0], matrix[3][0] };
        result.Row1 = { matrix[0][1], matrix[1][1], matrix[2][1], matrix[3][1] };
        result.Row2 = { matrix[0][2], matrix[1][2], matrix[2][2], matrix[3][2] };
        return result;
    }

    glm::mat4 AffineTransform3x4::ToMatrix() const
    {
        glm::mat4 result(1.0f);
        result[0] = { Row0.x, Row1.x, Row2.x, 0.0f };
        result[1] = { Row0.y, Row1.y, Row2.y, 0.0f };
        result[2] = { Row0.z, Row1.z, Row2.z, 0.0f };
        result[3] = { Row0.w, Row1.w, Row2.w, 1.0f };
        return result;
    }

    RenderWorld::RenderWorld(uint32_t initialCapacity) { Reserve(initialCapacity); }

    RenderInstanceHandle RenderWorld::CreateInstance(const RenderInstanceDesc& desc)
    {
        ScopedLock lock(m_Mutex);

        uint32_t slotIndex = 0;
        if (!m_FreeSlots.empty())
        {
            slotIndex = m_FreeSlots.back();
            m_FreeSlots.pop_back();
        }
        else
        {
            if (m_Slots.size() >= RenderInstanceHandle::MaxInstances)
                return {};
            slotIndex = static_cast<uint32_t>(m_Slots.size());
            m_Slots.emplace_back();
        }

        Slot& slot = m_Slots[slotIndex];
        const RenderInstanceHandle handle = RenderInstanceHandle::FromParts(slotIndex, slot.Generation);
        slot.Data = BuildInstanceData(desc, { handle.GetValue() });
        slot.RenderLayerOrder = desc.RenderLayerOrder;
        slot.TransformSettleQueued = false;
        slot.Alive = true;
        m_ActiveInstances++;
        QueueChange(slotIndex, handle, RenderWorldChangeType::Create, RenderWorldDirtyFlags::All);
        return handle;
    }

    bool RenderWorld::UpdateInstance(RenderInstanceHandle handle, const RenderInstanceDesc& desc)
    {
        ScopedLock lock(m_Mutex);
        if (!ValidateHandle(handle))
            return false;

        Slot& slot = m_Slots[handle.GetIndex()];
        const bool pendingCreate = slot.PendingChange != InvalidChangeIndex &&
                                   m_PendingChanges[slot.PendingChange].Type == RenderWorldChangeType::Create;
        const AffineTransform3x4 previous =
          slot.PendingChange == InvalidChangeIndex ? slot.Data.Transforms.Current : slot.Data.Transforms.Previous;
        slot.Data = BuildInstanceData(desc, { handle.GetValue() });
        slot.RenderLayerOrder = desc.RenderLayerOrder;
        slot.Data.Transforms.Previous = pendingCreate ? slot.Data.Transforms.Current : previous;
        SetTransformSettleRequired(handle.GetIndex(), !pendingCreate && !TransformsEqual(slot.Data.Transforms.Current, previous));
        QueueChange(handle.GetIndex(), handle, RenderWorldChangeType::Update, RenderWorldDirtyFlags::All);
        return true;
    }

    bool RenderWorld::UpdateTransform(RenderInstanceHandle handle, const glm::mat4& transform, const glm::vec4& boundingSphere)
    {
        ScopedLock lock(m_Mutex);
        if (!ValidateHandle(handle))
            return false;

        Slot& slot = m_Slots[handle.GetIndex()];
        const bool pendingCreate = slot.PendingChange != InvalidChangeIndex &&
                                   m_PendingChanges[slot.PendingChange].Type == RenderWorldChangeType::Create;
        const AffineTransform3x4 previous =
          slot.PendingChange == InvalidChangeIndex ? slot.Data.Transforms.Current : slot.Data.Transforms.Previous;
        slot.Data.Transforms.Current = AffineTransform3x4::FromMatrix(transform);
        slot.Data.Transforms.Previous = pendingCreate ? slot.Data.Transforms.Current : previous;
        slot.Data.Culling.BoundingSphere = boundingSphere;
        SetTransformSettleRequired(handle.GetIndex(), !pendingCreate && !TransformsEqual(slot.Data.Transforms.Current, previous));
        QueueChange(handle.GetIndex(), handle, RenderWorldChangeType::Update,
                    RenderWorldDirtyFlags::Transform | RenderWorldDirtyFlags::Bounds);
        return true;
    }

    bool RenderWorld::DestroyInstance(RenderInstanceHandle handle)
    {
        ScopedLock lock(m_Mutex);
        if (!ValidateHandle(handle))
            return false;

        Slot& slot = m_Slots[handle.GetIndex()];
        slot.Alive = false;
        m_ActiveInstances--;

        if (slot.PendingChange != InvalidChangeIndex &&
            m_PendingChanges[slot.PendingChange].Type == RenderWorldChangeType::Create)
        {
            m_PendingChanges[slot.PendingChange].Type = RenderWorldChangeType::Cancelled;
            m_PendingChanges[slot.PendingChange].DirtyFlags = RenderWorldDirtyFlags::None;
        }
        else
        {
            QueueChange(handle.GetIndex(), handle, RenderWorldChangeType::Destroy, RenderWorldDirtyFlags::None);
        }

        slot.Generation = NextGeneration(slot.Generation);
        m_DeferredFreeSlots.push_back(handle.GetIndex());
        return true;
    }

    bool RenderWorld::IsAlive(RenderInstanceHandle handle) const
    {
        ScopedLock lock(m_Mutex);
        return ValidateHandle(handle);
    }

    bool RenderWorld::TryGetInstance(RenderInstanceHandle handle, RenderInstanceData& output, int32_t* renderLayerOrder) const
    {
        ScopedLock lock(m_Mutex);
        if (!ValidateHandle(handle))
            return false;
        const Slot& slot = m_Slots[handle.GetIndex()];
        output = slot.Data;
        if (renderLayerOrder != nullptr)
            *renderLayerOrder = slot.RenderLayerOrder;
        return true;
    }

    uint32_t RenderWorld::GetActiveInstanceCount() const
    {
        ScopedLock lock(m_Mutex);
        return m_ActiveInstances;
    }

    uint32_t RenderWorld::GetCapacity() const
    {
        ScopedLock lock(m_Mutex);
        return static_cast<uint32_t>(m_Slots.capacity());
    }

    void RenderWorld::Reserve(uint32_t capacity)
    {
        ScopedLock lock(m_Mutex);
        const uint32_t clampedCapacity = std::min(capacity, RenderInstanceHandle::MaxInstances);
        m_Slots.reserve(clampedCapacity);
        m_FreeSlots.reserve(clampedCapacity);
        m_DeferredFreeSlots.reserve(clampedCapacity);
        m_TransformSettleSlots.reserve(clampedCapacity);
        m_PendingChanges.reserve(clampedCapacity);
    }

    void RenderWorld::DrainChanges(Vector<RenderWorldChange>& output)
    {
        ScopedLock lock(m_Mutex);
        output.clear();
        output.reserve(std::max(output.capacity(), m_PendingChanges.size()));
        for (const RenderWorldChange& change : m_PendingChanges)
        {
            if (change.Type != RenderWorldChangeType::Cancelled)
                output.push_back(change);

            const uint32_t slotIndex = change.Handle.GetIndex();
            if (slotIndex < m_Slots.size())
                m_Slots[slotIndex].PendingChange = InvalidChangeIndex;
        }
        m_PendingChanges.clear();

        for (uint32_t slotIndex : m_DeferredFreeSlots)
        {
            m_Slots[slotIndex].Data = {};
            m_Slots[slotIndex].RenderLayerOrder = 0;
            m_Slots[slotIndex].TransformSettleQueued = false;
            m_FreeSlots.push_back(slotIndex);
        }
        m_DeferredFreeSlots.clear();

        for (uint32_t slotIndex : m_TransformSettleSlots)
        {
            if (slotIndex >= m_Slots.size())
                continue;
            Slot& slot = m_Slots[slotIndex];
            if (!slot.Alive || !slot.TransformSettleQueued)
                continue;

            slot.TransformSettleQueued = false;
            slot.Data.Transforms.Previous = slot.Data.Transforms.Current;
            const RenderInstanceHandle handle = RenderInstanceHandle::FromParts(slotIndex, slot.Generation);
            QueueChange(slotIndex, handle, RenderWorldChangeType::Update, RenderWorldDirtyFlags::Transform);
        }
        m_TransformSettleSlots.clear();
    }

    float RenderWorld::GetLodBias(const RenderDrawRecord& record)
    {
        const int8_t quantized = static_cast<int8_t>(record.MaterialAndLodBias >> ResourceHandleBits);
        return static_cast<float>(quantized) / 16.0f;
    }

    RenderInstanceData RenderWorld::BuildInstanceData(const RenderInstanceDesc& desc, RenderObjectID fallbackObjectID)
    {
        RenderInstanceData data;
        data.Transforms.Current = AffineTransform3x4::FromMatrix(desc.Transform);
        data.Transforms.Previous = data.Transforms.Current;
        data.Culling.BoundingSphere = desc.BoundingSphere;

        const uint32_t meshHandle = desc.MeshHandle & ResourceHandleMask;
        const uint32_t materialHandle = desc.MaterialHandle & ResourceHandleMask;
        const int32_t quantizedBias = std::clamp(static_cast<int32_t>(std::lround(desc.LodBias * 16.0f)), -128, 127);
        data.Draw.MeshAndFlags = meshHandle | (static_cast<uint32_t>(static_cast<uint8_t>(desc.Flags)) << ResourceHandleBits);
        data.Draw.MaterialAndLodBias = materialHandle | (static_cast<uint32_t>(static_cast<uint8_t>(quantizedBias)) << ResourceHandleBits);
        data.Draw.VisibilityLayerMask = desc.VisibilityLayers.Value;
        data.Draw.ObjectID = desc.ObjectID.IsValid() ? desc.ObjectID.Value : fallbackObjectID.Value;
        return data;
    }

    uint32_t RenderWorld::NextGeneration(uint32_t generation)
    {
        generation = (generation + 1u) & RenderInstanceHandle::GenerationMask;
        return generation == 0 ? 1u : generation;
    }

    bool RenderWorld::TransformsEqual(const AffineTransform3x4& first, const AffineTransform3x4& second)
    {
        return first.Row0 == second.Row0 && first.Row1 == second.Row1 && first.Row2 == second.Row2;
    }

    void RenderWorld::SetTransformSettleRequired(uint32_t slotIndex, bool required)
    {
        Slot& slot = m_Slots[slotIndex];
        if (!required)
        {
            slot.TransformSettleQueued = false;
            return;
        }
        if (slot.TransformSettleQueued)
            return;
        slot.TransformSettleQueued = true;
        m_TransformSettleSlots.push_back(slotIndex);
    }

    void RenderWorld::QueueChange(uint32_t slotIndex, RenderInstanceHandle handle, RenderWorldChangeType type,
                                  RenderWorldDirtyFlags dirtyFlags)
    {
        Slot& slot = m_Slots[slotIndex];
        if (slot.PendingChange != InvalidChangeIndex)
        {
            RenderWorldChange& pending = m_PendingChanges[slot.PendingChange];
            pending.Data = slot.Data;
            pending.RenderLayerOrder = slot.RenderLayerOrder;
            pending.DirtyFlags = pending.DirtyFlags | dirtyFlags;
            if (pending.Type != RenderWorldChangeType::Create)
                pending.Type = type;
            return;
        }

        slot.PendingChange = static_cast<uint32_t>(m_PendingChanges.size());
        m_PendingChanges.push_back({ handle, type, dirtyFlags, slot.Data, slot.RenderLayerOrder });
    }

    bool RenderWorld::ValidateHandle(RenderInstanceHandle handle) const
    {
        if (!handle.IsValid() || handle.GetIndex() >= m_Slots.size())
            return false;
        const Slot& slot = m_Slots[handle.GetIndex()];
        return slot.Alive && slot.Generation == handle.GetGeneration();
    }

} // namespace Crowny
