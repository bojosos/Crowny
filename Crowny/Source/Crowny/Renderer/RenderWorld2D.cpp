#include "cwpch.h"

#include "Crowny/Memory/FrameVector.h"
#include "Crowny/Renderer/RenderWorld2D.h"

#include <algorithm>

namespace Crowny
{
    namespace
    {
        bool Equal(const AffineTransform3x4& a, const AffineTransform3x4& b) { return a.Row0 == b.Row0 && a.Row1 == b.Row1 && a.Row2 == b.Row2; }
    } // namespace

    RenderWorld2D::RenderWorld2D(uint32_t capacity) { Reserve(capacity); }

    void RenderWorld2D::Reserve(uint32_t capacity)
    {
        capacity = std::min(capacity, RenderHandle2D::MaxInstances);
        m_Slots.reserve(capacity);
        m_Free.reserve(capacity);
        m_Retired.reserve(capacity);
        m_Moving.reserve(capacity);
        m_Changes.reserve(capacity);
    }

    bool RenderWorld2D::IsAlive(RenderHandle2D handle) const
    {
        return handle && handle.GetIndex() < m_Slots.size() && m_Slots[handle.GetIndex()].Alive &&
               m_Slots[handle.GetIndex()].Generation == handle.GetGeneration();
    }

    void RenderWorld2D::Assign(Slot& slot, const RenderInstance2DDesc& desc)
    {
        slot.Value.Data.Transform = AffineTransform3x4::FromMatrix(desc.Transform);
        slot.Value.Data.Color = desc.Color;
        slot.Value.Data.UvRect = desc.UvRect;
        slot.Value.TextureResource = desc.TextureResource;
        slot.Value.VisibilityLayers = desc.VisibilityLayers;
        slot.Value.ObjectID = desc.ObjectID;
        slot.Value.SortingLayer = desc.SortingLayer;
        slot.Value.OrderInLayer = desc.OrderInLayer;
        slot.Value.Visible = desc.Visible;
    }

    RenderHandle2D RenderWorld2D::Create(const RenderInstance2DDesc& desc)
    {
        uint32_t index;
        if (!m_Free.empty())
        {
            index = m_Free.back();
            m_Free.pop_back();
        }
        else
        {
            if (m_Slots.size() == RenderHandle2D::MaxInstances)
                return {};
            index = static_cast<uint32_t>(m_Slots.size());
            m_Slots.emplace_back();
        }
        Slot& slot = m_Slots[index];
        slot.Alive = true;
        slot.NeedsSettle = false;
        slot.Value.Handle = RenderHandle2D::FromParts(index, slot.Generation);
        Assign(slot, desc);
        slot.Value.Data.PreviousTransform = slot.Value.Data.Transform;
        ++m_ActiveCount;
        Queue(index, RenderChange2DType::Create);
        return slot.Value.Handle;
    }

    bool RenderWorld2D::Update(RenderHandle2D handle, const RenderInstance2DDesc& desc)
    {
        if (!IsAlive(handle))
            return false;
        Slot& slot = m_Slots[handle.GetIndex()];
        const RenderChange2D& old = slot.Value;
        const AffineTransform3x4 transform = AffineTransform3x4::FromMatrix(desc.Transform);
        const bool transformChanged = !Equal(old.Data.Transform, transform);
        if (!transformChanged && old.Data.Color == desc.Color && old.Data.UvRect == desc.UvRect && old.TextureResource == desc.TextureResource &&
            old.VisibilityLayers == desc.VisibilityLayers && old.ObjectID.Value == desc.ObjectID.Value && old.SortingLayer == desc.SortingLayer &&
            old.OrderInLayer == desc.OrderInLayer && old.Visible == desc.Visible)
            return true;
        Assign(slot, desc);
        const bool pendingCreate = slot.PendingChange && slot.Value.Type == RenderChange2DType::Create;
        if (pendingCreate)
            slot.Value.Data.PreviousTransform = transform;
        else if (transformChanged && !slot.NeedsSettle)
        {
            slot.NeedsSettle = true;
            m_Moving.push_back(handle.GetIndex());
        }
        Queue(handle.GetIndex(), pendingCreate ? RenderChange2DType::Create : RenderChange2DType::Update);
        return true;
    }

    bool RenderWorld2D::Destroy(RenderHandle2D handle)
    {
        if (!IsAlive(handle))
            return false;
        Slot& slot = m_Slots[handle.GetIndex()];
        const bool pendingCreate = slot.PendingChange && slot.Value.Type == RenderChange2DType::Create;
        Queue(handle.GetIndex(), pendingCreate ? RenderChange2DType::Cancelled : RenderChange2DType::Destroy);
        slot.Alive = false;
        --m_ActiveCount;
        m_Retired.push_back(handle.GetIndex());
        return true;
    }

    void RenderWorld2D::Queue(uint32_t index, RenderChange2DType type)
    {
        Slot& slot = m_Slots[index];
        slot.Value.Type = type;
        if (!slot.PendingChange)
        {
            slot.PendingChange = true;
            m_Changes.push_back(index);
        }
    }

    void RenderWorld2D::BeginFrame(uint64_t frameNumber)
    {
        if (m_HasFrame && m_FrameNumber == frameNumber)
            return;
        m_HasFrame = true;
        m_FrameNumber = frameNumber;
        for (uint32_t index : m_Moving)
        {
            Slot& slot = m_Slots[index];
            if (!slot.NeedsSettle)
                continue;
            slot.NeedsSettle = false;
            if (!slot.Alive)
                continue;
            slot.Value.Data.PreviousTransform = slot.Value.Data.Transform;
            Queue(index, RenderChange2DType::Update);
        }
        m_Moving.clear();
    }

    template <typename Append> void RenderWorld2D::DrainTo(Append&& append)
    {
        // Queue each slot once and copy its final value only at publication.
        // Retired slots cannot be reused until every pending value is published.
        for (uint32_t index : m_Changes)
        {
            Slot& slot = m_Slots[index];
            if (slot.Value.Type != RenderChange2DType::Cancelled)
                append(slot.Value);
            slot.PendingChange = false;
        }
        m_Changes.clear();
        for (uint32_t index : m_Retired)
        {
            Slot& slot = m_Slots[index];
            slot.Value = {};
            slot.NeedsSettle = false;
            // Exhausted generations permanently retire their index rather than
            // making an ancient handle valid again after wraparound.
            if (slot.Generation < RenderHandle2D::MaxGeneration)
            {
                ++slot.Generation;
                m_Free.push_back(index);
            }
        }
        m_Retired.clear();
    }

    void RenderWorld2D::DrainChanges(Vector<RenderChange2D>& output)
    {
        output.clear();
        output.reserve(m_Changes.size());
        DrainTo([&](const RenderChange2D& change) { output.push_back(change); });
    }

    void RenderWorld2D::DrainChanges(FrameVector<RenderChange2D>& output)
    {
        for (RenderChange2D& change : output)
            change.TextureResource.Reset();
        output.Reset();
        output.Reserve(m_Changes.size());
        DrainTo([&](const RenderChange2D& change) { output.Acquire() = change; });
    }
} // namespace Crowny
