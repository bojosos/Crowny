#pragma once

#include "Crowny/Common/Types.h"
#include "Crowny/Renderer/RenderLight.h"

namespace Crowny
{
    struct ShadowAtlasAllocation
    {
        RenderLightHandle Light;
        uint16_t X = 0;
        uint16_t Y = 0;
        uint16_t Size = 0;

        bool IsValid() const { return Light.IsValid() && Size != 0; }
    };

    // Power-of-two square allocator for spot-light shadow maps. Released
    // regions retire against the frame timeline before becoming reusable.
    class ShadowAtlasAllocator
    {
    public:
        explicit ShadowAtlasAllocator(uint32_t atlasSize = 2048, uint32_t minimumBlockSize = 128);

        ShadowAtlasAllocation Acquire(RenderLightHandle light, uint32_t requestedResolution);
        bool Release(RenderLightHandle light, uint64_t retireValue);
        void Collect(uint64_t completedValue);
        bool TryGet(RenderLightHandle light, ShadowAtlasAllocation& output) const;

        uint32_t GetAtlasSize() const { return m_AtlasSize; }
        uint32_t GetAllocationCount() const { return static_cast<uint32_t>(m_Allocations.size()); }

    private:
        struct Block
        {
            uint16_t X = 0;
            uint16_t Y = 0;
        };

        struct RetiredAllocation
        {
            ShadowAtlasAllocation Allocation;
            uint64_t RetireValue = 0;
        };

        uint32_t ResolutionToLevel(uint32_t resolution) const;
        uint32_t LevelToResolution(uint32_t level) const;
        void FreeBlock(ShadowAtlasAllocation allocation);

        uint32_t m_AtlasSize = 0;
        uint32_t m_MinimumBlockSize = 0;
        Vector<Vector<Block>> m_FreeBlocks;
        UnorderedMap<uint32_t, ShadowAtlasAllocation> m_Allocations;
        Vector<RetiredAllocation> m_RetiredAllocations;
    };

    struct ShadowUpdateRequest
    {
        RenderLightHandle Light;
        LightType Type = LightType::Spot;
        uint32_t Resolution = 1024;
        float Importance = 1.0f;
        bool RequiresRedraw = true;
        LightShadowSettings Settings;
    };

    struct ShadowUpdateBudget
    {
        uint32_t MaximumLocalUpdates = 4;
        uint64_t MaximumPixels = 4ull * 1024ull * 1024ull;
    };

    inline bool RequiresShadowCacheRedraw(const LightShadowSettings& settings, bool lightOrSettingsChanged, uint64_t casterRevision,
                                          uint64_t cachedCasterRevision)
    {
        return !settings.CacheStaticCasters || lightOrSettingsChanged || casterRevision != cachedCasterRevision;
    }

    class ShadowUpdateScheduler
    {
    public:
        void Schedule(const ShadowUpdateRequest* requests, uint32_t requestCount, const ShadowUpdateBudget& budget,
                      Vector<RenderLightHandle>& scheduled, uint64_t& scheduledPixels);

    private:
        struct Candidate
        {
            const ShadowUpdateRequest* Request = nullptr;
            float Score = 0.0f;
            uint32_t SourceOrdinal = 0;
        };

        Vector<Candidate> m_Candidates;
    };
} // namespace Crowny
