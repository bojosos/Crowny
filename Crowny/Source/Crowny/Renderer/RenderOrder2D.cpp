#include "cwpch.h"

#include "Crowny/Renderer/RenderOrder2D.h"

#include <algorithm>
#include <bit>
#include <cmath>

namespace Crowny
{
    namespace
    {
        uint32_t DepthKey(float depth)
        {
            // Normalize non-finite depths and signed zero so every backend has
            // the same total order and comparison sorting agrees with radix sorting.
            if (!std::isfinite(depth) || depth == 0.0f)
                depth = 0.0f;
            const uint32_t bits = std::bit_cast<uint32_t>(depth);
            return ~(bits ^ ((bits & 0x80000000u) ? 0xffffffffu : 0x80000000u));
        }

        uint32_t Word(const Renderable2DOrder& item, uint32_t word)
        {
            switch (word)
            {
            case 0:
                return item.Index;
            case 1:
                return static_cast<uint32_t>(item.Type);
            case 2:
                return item.StableOrder;
            case 3:
                return DepthKey(item.ViewDepth);
            case 4:
                return static_cast<uint32_t>(item.OrderInLayer) ^ 0x80000000u;
            default:
                return static_cast<uint32_t>(item.SortingLayer) ^ 0x80000000u;
            }
        }
    } // namespace

    bool Renderable2DOrderLess(const Renderable2DOrder& first, const Renderable2DOrder& second)
    {
        for (uint32_t word = 6; word-- > 0;)
        {
            const uint32_t a = Word(first, word), b = Word(second, word);
            if (a != b)
                return a < b;
        }
        return false;
    }

    void RenderOrder2D::Sort(std::span<Renderable2DOrder> items)
    {
        if (items.size() == m_Input.size() && std::equal(items.begin(), items.end(), m_Input.begin()))
        {
            std::copy(m_Sorted.begin(), m_Sorted.end(), items.begin());
            return;
        }
        m_Input.assign(items.begin(), items.end());
        m_Sorted.assign(items.begin(), items.end());
        m_Scratch.resize(items.size());
        ++m_SortCount;
        for (uint32_t word = 0; word < 6; ++word)
        {
            for (uint32_t shift = 0; shift < 32; shift += 8)
            {
                Array<size_t, 256> counts{};
                for (const auto& item : m_Sorted)
                    ++counts[(Word(item, word) >> shift) & 255];
                if (std::find(counts.begin(), counts.end(), items.size()) != counts.end())
                    continue;
                size_t prefix = 0;
                for (size_t& count : counts)
                {
                    const size_t size = count;
                    count = prefix;
                    prefix += size;
                }
                for (const auto& item : m_Sorted)
                    m_Scratch[counts[(Word(item, word) >> shift) & 255]++] = item;
                m_Sorted.swap(m_Scratch);
            }
        }
        std::copy(m_Sorted.begin(), m_Sorted.end(), items.begin());
    }
} // namespace Crowny
