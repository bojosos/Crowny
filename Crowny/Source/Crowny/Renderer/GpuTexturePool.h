#pragma once

#include "Crowny/RenderAPI/Texture.h"

#include <mutex>

namespace Crowny
{
    struct GpuTexturePoolStats
    {
        uint64_t Created = 0;
        uint64_t Reused = 0;
        uint64_t Rejected = 0;
        uint64_t RetainedBytes = 0; // Estimated texel storage; backend alignment is not included.
        uint32_t AvailableTextures = 0;
        uint32_t RetiredTextures = 0;
    };

    // Reuses whole texture objects after their frames-in-flight retirement
    // window. Debug names do not affect compatibility; every resource-creation
    // field in TextureDesc does.
    class GpuTexturePool
    {
    public:
        explicit GpuTexturePool(uint32_t framesInFlight = 2,
                                uint64_t retainedByteBudget = 64ull * 1024ull * 1024ull);

        void BeginFrame(uint64_t frameNumber);
        Ref<Texture> Acquire(const TextureDesc& desc);
        void Release(Ref<Texture>&& texture);

        void SetRetainedByteBudget(uint64_t byteBudget);
        void Trim();
        GpuTexturePoolStats GetStats() const;

    private:
        struct TextureKey
        {
            uint32_t Type = 0;
            uint32_t Shape = 0;
            uint32_t Flags = 0;
            uint32_t MipLevels = 0;
            uint32_t Samples = 1;
            uint32_t Faces = 1;
            uint32_t Width = 1;
            uint32_t Height = 1;
            uint32_t Depth = 1;
            uint32_t Usage = 0;
            uint32_t Format = 0;
            uint64_t Size = 0;

            bool operator==(const TextureKey& other) const = default;
        };

        struct TextureKeyHash
        {
            size_t operator()(const TextureKey& key) const;
        };

        struct Entry
        {
            TextureKey Key;
            Ref<Texture> TextureResource;
            uint64_t RetiredFrame = 0;
        };

        static TextureKey MakeKey(const TextureDesc& desc);
        Ref<Texture> TryAcquire(const TextureKey& key);
        void MakeReady();
        void TrimLocked();

        uint32_t m_FramesInFlight = 2;
        uint64_t m_CurrentFrame = 0;
        uint64_t m_RetainedByteBudget = 0;
        mutable std::mutex m_Mutex;
        UnorderedMap<TextureKey, Vector<Ref<Texture>>, TextureKeyHash> m_Available;
        Vector<Entry> m_Retired;
        GpuTexturePoolStats m_Stats;
    };
} // namespace Crowny
