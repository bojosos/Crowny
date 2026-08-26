#include "cwpch.h"

#include "Crowny/Renderer/ShadowAtlas.h"

#include <bit>

namespace Crowny
{
    namespace
    {
        bool IsPowerOfTwo(uint32_t value) { return value != 0 && (value & (value - 1u)) == 0; }

        uint64_t ShadowPixelCost(const ShadowUpdateRequest& request)
        {
            const uint64_t resolution = std::max(request.Resolution, 1u);
            return resolution * resolution * (request.Type == LightType::Point ? 6ull : 1ull);
        }
    } // namespace

    ShadowAtlasAllocator::ShadowAtlasAllocator(uint32_t atlasSize, uint32_t minimumBlockSize)
      : m_AtlasSize(atlasSize), m_MinimumBlockSize(minimumBlockSize)
    {
        CW_ENGINE_ASSERT(IsPowerOfTwo(atlasSize) && IsPowerOfTwo(minimumBlockSize) && minimumBlockSize <= atlasSize);
        const uint32_t levelCount = static_cast<uint32_t>(std::log2(atlasSize / minimumBlockSize)) + 1u;
        m_FreeBlocks.resize(levelCount);
        m_FreeBlocks[0].push_back({ 0, 0 });
    }

    ShadowAtlasAllocation ShadowAtlasAllocator::Acquire(RenderLightHandle light, uint32_t requestedResolution)
    {
        if (!light.IsValid())
            return {};
        const uint32_t targetLevel = ResolutionToLevel(requestedResolution);
        const uint32_t resolution = LevelToResolution(targetLevel);
        const auto existing = m_Allocations.find(light.GetValue());
        if (existing != m_Allocations.end())
        {
            if (existing->second.Size == resolution)
                return existing->second;
            FreeBlock(existing->second);
            m_Allocations.erase(existing);
        }

        uint32_t sourceLevel = targetLevel;
        while (sourceLevel > 0 && m_FreeBlocks[sourceLevel].empty())
            sourceLevel--;
        if (m_FreeBlocks[sourceLevel].empty())
            return {};

        Block block = m_FreeBlocks[sourceLevel].back();
        m_FreeBlocks[sourceLevel].pop_back();
        while (sourceLevel < targetLevel)
        {
            const uint32_t childLevel = sourceLevel + 1u;
            const uint16_t childSize = static_cast<uint16_t>(LevelToResolution(childLevel));
            m_FreeBlocks[childLevel].push_back({ static_cast<uint16_t>(block.X + childSize), block.Y });
            m_FreeBlocks[childLevel].push_back({ block.X, static_cast<uint16_t>(block.Y + childSize) });
            m_FreeBlocks[childLevel].push_back({ static_cast<uint16_t>(block.X + childSize),
                                                 static_cast<uint16_t>(block.Y + childSize) });
            sourceLevel = childLevel;
        }

        ShadowAtlasAllocation allocation{ light, block.X, block.Y, static_cast<uint16_t>(resolution) };
        m_Allocations.emplace(light.GetValue(), allocation);
        return allocation;
    }

    bool ShadowAtlasAllocator::Release(RenderLightHandle light, uint64_t retireValue)
    {
        const auto existing = m_Allocations.find(light.GetValue());
        if (existing == m_Allocations.end())
            return false;
        m_RetiredAllocations.push_back({ existing->second, retireValue });
        m_Allocations.erase(existing);
        return true;
    }

    void ShadowAtlasAllocator::Collect(uint64_t completedValue)
    {
        for (auto retired = m_RetiredAllocations.begin(); retired != m_RetiredAllocations.end();)
        {
            if (retired->RetireValue > completedValue)
            {
                ++retired;
                continue;
            }
            FreeBlock(retired->Allocation);
            retired = m_RetiredAllocations.erase(retired);
        }
    }

    bool ShadowAtlasAllocator::TryGet(RenderLightHandle light, ShadowAtlasAllocation& output) const
    {
        const auto existing = m_Allocations.find(light.GetValue());
        if (existing == m_Allocations.end())
            return false;
        output = existing->second;
        return true;
    }

    uint32_t ShadowAtlasAllocator::ResolutionToLevel(uint32_t resolution) const
    {
        resolution = std::clamp(std::bit_ceil(std::max(resolution, 1u)), m_MinimumBlockSize, m_AtlasSize);
        return static_cast<uint32_t>(std::log2(m_AtlasSize / resolution));
    }

    uint32_t ShadowAtlasAllocator::LevelToResolution(uint32_t level) const { return m_AtlasSize >> level; }

    void ShadowAtlasAllocator::FreeBlock(ShadowAtlasAllocation allocation)
    {
        uint32_t level = ResolutionToLevel(allocation.Size);
        Block block{ allocation.X, allocation.Y };
        while (level > 0)
        {
            const uint16_t blockSize = static_cast<uint16_t>(LevelToResolution(level));
            const uint16_t parentSize = static_cast<uint16_t>(blockSize * 2u);
            const uint16_t parentX = static_cast<uint16_t>((block.X / parentSize) * parentSize);
            const uint16_t parentY = static_cast<uint16_t>((block.Y / parentSize) * parentSize);
            const std::array<Block, 4> siblings{ Block{ parentX, parentY }, Block{ static_cast<uint16_t>(parentX + blockSize), parentY },
                                                 Block{ parentX, static_cast<uint16_t>(parentY + blockSize) },
                                                 Block{ static_cast<uint16_t>(parentX + blockSize),
                                                        static_cast<uint16_t>(parentY + blockSize) } };

            Vector<Block>& freeBlocks = m_FreeBlocks[level];
            Vector<size_t> found;
            found.reserve(3);
            for (const Block& sibling : siblings)
            {
                if (sibling.X == block.X && sibling.Y == block.Y)
                    continue;
                const auto match = std::find_if(freeBlocks.begin(), freeBlocks.end(), [&](const Block& candidate) {
                    return candidate.X == sibling.X && candidate.Y == sibling.Y;
                });
                if (match == freeBlocks.end())
                {
                    found.clear();
                    break;
                }
                found.push_back(static_cast<size_t>(std::distance(freeBlocks.begin(), match)));
            }
            if (found.size() != 3)
                break;
            std::sort(found.rbegin(), found.rend());
            for (size_t index : found)
                freeBlocks.erase(freeBlocks.begin() + index);
            block = { parentX, parentY };
            level--;
        }
        m_FreeBlocks[level].push_back(block);
    }

    void ShadowUpdateScheduler::Schedule(const ShadowUpdateRequest* requests, uint32_t requestCount, const ShadowUpdateBudget& budget,
                                         Vector<RenderLightHandle>& scheduled, uint64_t& scheduledPixels)
    {
        scheduled.clear();
        scheduledPixels = 0;
        m_Candidates.clear();
        m_Candidates.reserve(requestCount);
        for (uint32_t index = 0; requests != nullptr && index < requestCount; index++)
            if (requests[index].Light.IsValid() && requests[index].RequiresRedraw && requests[index].Type != LightType::Directional)
                m_Candidates.push_back(
                  { &requests[index], requests[index].Importance / static_cast<float>(ShadowPixelCost(requests[index])), index });
        std::sort(m_Candidates.begin(), m_Candidates.end(), [](const Candidate& first, const Candidate& second) {
            if (first.Score > second.Score)
                return true;
            if (second.Score > first.Score)
                return false;
            return first.SourceOrdinal < second.SourceOrdinal;
        });

        for (const Candidate& candidate : m_Candidates)
        {
            const ShadowUpdateRequest* request = candidate.Request;
            if (scheduled.size() >= budget.MaximumLocalUpdates)
                break;
            const uint64_t pixels = ShadowPixelCost(*request);
            if (scheduledPixels + pixels > budget.MaximumPixels)
                continue;
            scheduled.push_back(request->Light);
            scheduledPixels += pixels;
        }
    }
} // namespace Crowny
