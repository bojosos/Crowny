#pragma once

#include "Crowny/Common/StdHeaders.h"

#include <span>

namespace Crowny
{
    enum class Renderable2DType : uint8_t
    {
        Sprite,
        Text
    };

    struct Renderable2DOrder
    {
        Renderable2DType Type = Renderable2DType::Sprite;
        uint32_t Index = 0;
        int32_t SortingLayer = 0;
        int32_t OrderInLayer = 0;
        uint32_t StableOrder = 0;
        float ViewDepth = 0.0f;
        bool operator==(const Renderable2DOrder&) const = default;
    };

    bool Renderable2DOrderLess(const Renderable2DOrder& first, const Renderable2DOrder& second);

    // Per-view retained radix scratch. Reuses the last result when all keys and
    // source indices are unchanged, even when instance transforms were updated.
    class RenderOrder2D final
    {
    public:
        void Sort(std::span<Renderable2DOrder> items);
        uint64_t GetSortCount() const { return m_SortCount; }

    private:
        Vector<Renderable2DOrder> m_Input;
        Vector<Renderable2DOrder> m_Sorted;
        Vector<Renderable2DOrder> m_Scratch;
        uint64_t m_SortCount = 0;
    };
} // namespace Crowny
