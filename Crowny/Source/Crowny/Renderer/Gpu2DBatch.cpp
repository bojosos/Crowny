#include "cwpch.h"

#include "Crowny/Renderer/Gpu2DBatch.h"

namespace Crowny
{
    void Gpu2DBatchBuilder::Reserve(uint32_t itemCount) { m_Order.reserve(itemCount); }

    bool Gpu2DBatchBuilder::BatchLess(const Gpu2DBatchKey& first, const Gpu2DBatchKey& second)
    {
        if (first.Pipeline != second.Pipeline)
            return first.Pipeline < second.Pipeline;
        if (first.MaterialTemplate != second.MaterialTemplate)
            return first.MaterialTemplate < second.MaterialTemplate;
        if (first.Alpha != second.Alpha)
            return first.Alpha < second.Alpha;
        return first.Primitive < second.Primitive;
    }

    void Gpu2DBatchBuilder::Build(const Gpu2DDrawItem* items, uint32_t itemCount, Gpu2DOrderingMode ordering, Gpu2DDrawList& output)
    {
        output.Clear();
        m_Order.clear();
        m_Order.reserve(std::max<size_t>(m_Order.capacity(), itemCount));
        for (uint32_t index = 0; items != nullptr && index < itemCount; index++)
            m_Order.push_back(index);

        std::stable_sort(m_Order.begin(), m_Order.end(), [&](uint32_t firstIndex, uint32_t secondIndex) {
            const Gpu2DDrawItem& first = items[firstIndex];
            const Gpu2DDrawItem& second = items[secondIndex];
            if (first.SortingLayer != second.SortingLayer)
                return first.SortingLayer < second.SortingLayer;
            if (ordering == Gpu2DOrderingMode::BatchOptimized && !(first.Batch == second.Batch))
                return BatchLess(first.Batch, second.Batch);
            if (first.OrderInLayer != second.OrderInLayer)
                return first.OrderInLayer < second.OrderInLayer;
            if (first.StableOrder != second.StableOrder)
                return first.StableOrder < second.StableOrder;
            return firstIndex < secondIndex;
        });

        output.Instances.reserve(itemCount);
        output.Runs.reserve(itemCount);
        int32_t previousSortingLayer = 0;
        bool hasPreviousItem = false;
        for (uint32_t index : m_Order)
        {
            const Gpu2DDrawItem& item = items[index];
            if (!hasPreviousItem || previousSortingLayer != item.SortingLayer || !(output.Runs.back().Batch == item.Batch))
                output.Runs.push_back({ item.Batch, static_cast<uint32_t>(output.Instances.size()), 0u });
            output.Instances.push_back(item.Data);
            output.Runs.back().InstanceCount++;
            previousSortingLayer = item.SortingLayer;
            hasPreviousItem = true;
        }
    }
} // namespace Crowny
