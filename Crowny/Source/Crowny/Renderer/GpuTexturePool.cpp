#include "cwpch.h"

#include "Crowny/Renderer/GpuTexturePool.h"

namespace Crowny
{
    namespace
    {
        constexpr uint32_t SRGB_FLAG = 1u << 0u;
        constexpr uint32_t READ_WRITE_FLAG = 1u << 1u;
        constexpr uint32_t GENERATE_MIPS_FLAG = 1u << 2u;

        void HashCombine(size_t& hash, uint64_t value) { hash ^= static_cast<size_t>(value) + 0x9e3779b97f4a7c15ull + (hash << 6u) + (hash >> 2u); }

        uint64_t EstimateTextureBytes(const TextureDesc& desc)
        {
            if (!PixelUtils::IsValidFormat(desc.Format))
                return 0;

            const uint32_t width = std::max(desc.Width, 1u);
            const uint32_t height = std::max(desc.Height, 1u);
            const uint32_t depth = std::max(desc.Depth, 1u);
            const uint32_t faces = std::max(desc.Faces, 1u);
            const uint32_t samples = std::max(desc.Samples, 1u);
            const size_t chainSize = PixelUtils::GetMipChainSize(width, height, depth, desc.Format, desc.MipLevels + 1u, faces);
            if (chainSize > std::numeric_limits<uint64_t>::max() / samples)
                return std::numeric_limits<uint64_t>::max();
            return static_cast<uint64_t>(chainSize) * samples;
        }
    } // namespace

    GpuTexturePool::GpuTexturePool(uint32_t framesInFlight, uint64_t retainedByteBudget)
      : m_FramesInFlight(std::max(framesInFlight, 1u)), m_RetainedByteBudget(retainedByteBudget)
    {
    }

    void GpuTexturePool::BeginFrame(uint64_t frameNumber)
    {
        std::scoped_lock lock(m_Mutex);
        m_CurrentFrame = frameNumber;
        MakeReady();
    }

    Ref<Texture> GpuTexturePool::Acquire(const TextureDesc& desc)
    {
        if (IsReusableDescriptor(desc))
        {
            Ref<Texture> pooled = TryAcquire(MakeKey(desc));
            if (pooled)
                return pooled;
        }

        Ref<Texture> created = Texture::Create(desc);
        if (created)
        {
            std::scoped_lock lock(m_Mutex);
            m_Stats.Created++;
        }
        return created;
    }

    void GpuTexturePool::Release(Ref<Texture>&& texture)
    {
        if (!texture)
            return;

        const TextureDesc& desc = texture->GetDesc();
        if (!IsReusableDescriptor(desc))
        {
            std::scoped_lock lock(m_Mutex);
            m_Stats.Rejected++;
            return;
        }

        const TextureKey key = MakeKey(desc);
        std::scoped_lock lock(m_Mutex);
        if (texture->GetRefCount() != 1 || key.Size == 0 || m_Stats.RetainedBytes > m_RetainedByteBudget || key.Size > m_RetainedByteBudget ||
            key.Size > m_RetainedByteBudget - m_Stats.RetainedBytes)
        {
            m_Stats.Rejected++;
            return;
        }

        m_Retired.push_back({ key, std::move(texture), m_CurrentFrame });
        m_Stats.RetainedBytes += key.Size;
        m_Stats.RetiredTextures++;
    }

    void GpuTexturePool::SetRetainedByteBudget(uint64_t byteBudget)
    {
        std::scoped_lock lock(m_Mutex);
        m_RetainedByteBudget = byteBudget;
        TrimLocked();
    }

    void GpuTexturePool::Trim()
    {
        std::scoped_lock lock(m_Mutex);
        m_Available.clear();
        m_Retired.clear();
        m_Stats.RetainedBytes = 0;
        m_Stats.AvailableTextures = 0;
        m_Stats.RetiredTextures = 0;
    }

    GpuTexturePoolStats GpuTexturePool::GetStats() const
    {
        std::scoped_lock lock(m_Mutex);
        return m_Stats;
    }

    size_t GpuTexturePool::TextureKeyHash::operator()(const TextureKey& key) const
    {
        size_t hash = static_cast<size_t>(key.Type);
        HashCombine(hash, key.Shape);
        HashCombine(hash, key.Flags);
        HashCombine(hash, key.MipLevels);
        HashCombine(hash, key.Samples);
        HashCombine(hash, key.Faces);
        HashCombine(hash, key.Width);
        HashCombine(hash, key.Height);
        HashCombine(hash, key.Depth);
        HashCombine(hash, key.Usage);
        HashCombine(hash, key.Format);
        HashCombine(hash, key.Size);
        return hash;
    }

    bool GpuTexturePool::IsReusableDescriptor(const TextureDesc& desc)
    {
        return desc.Width != 0 && desc.Height != 0 && desc.Depth != 0 && desc.Samples != 0 && desc.Faces != 0 &&
               PixelUtils::IsValidFormat(desc.Format);
    }

    GpuTexturePool::TextureKey GpuTexturePool::MakeKey(const TextureDesc& desc)
    {
        uint32_t flags = 0;
        if (desc.sRGB)
            flags |= SRGB_FLAG;
        if (desc.ReadWrite)
            flags |= READ_WRITE_FLAG;
        if (desc.GenerateMipmaps)
            flags |= GENERATE_MIPS_FLAG;

        return { static_cast<uint32_t>(desc.Type),
                 static_cast<uint32_t>(desc.Shape),
                 flags,
                 desc.MipLevels,
                 std::max(desc.Samples, 1u),
                 std::max(desc.Faces, 1u),
                 std::max(desc.Width, 1u),
                 std::max(desc.Height, 1u),
                 std::max(desc.Depth, 1u),
                 static_cast<uint32_t>(desc.Usage),
                 static_cast<uint32_t>(desc.Format),
                 EstimateTextureBytes(desc) };
    }

    Ref<Texture> GpuTexturePool::TryAcquire(const TextureKey& key)
    {
        std::scoped_lock lock(m_Mutex);
        MakeReady();
        const auto found = m_Available.find(key);
        if (found == m_Available.end() || found->second.empty())
            return nullptr;

        Ref<Texture> result = std::move(found->second.back());
        found->second.pop_back();
        if (found->second.empty())
            m_Available.erase(found);
        m_Stats.Reused++;
        m_Stats.RetainedBytes -= key.Size;
        m_Stats.AvailableTextures--;
        return result;
    }

    void GpuTexturePool::MakeReady()
    {
        for (size_t index = 0; index < m_Retired.size();)
        {
            Entry& entry = m_Retired[index];
            if (m_CurrentFrame < entry.RetiredFrame || m_CurrentFrame - entry.RetiredFrame < m_FramesInFlight)
            {
                index++;
                continue;
            }

            m_Available[entry.Key].push_back(std::move(entry.TextureResource));
            m_Retired[index] = std::move(m_Retired.back());
            m_Retired.pop_back();
            m_Stats.RetiredTextures--;
            m_Stats.AvailableTextures++;
        }
    }

    void GpuTexturePool::TrimLocked()
    {
        for (auto available = m_Available.begin(); available != m_Available.end() && m_Stats.RetainedBytes > m_RetainedByteBudget;)
        {
            Vector<Ref<Texture>>& textures = available->second;
            while (!textures.empty() && m_Stats.RetainedBytes > m_RetainedByteBudget)
            {
                textures.pop_back();
                m_Stats.RetainedBytes -= available->first.Size;
                m_Stats.AvailableTextures--;
            }
            available = textures.empty() ? m_Available.erase(available) : std::next(available);
        }

        while (!m_Retired.empty() && m_Stats.RetainedBytes > m_RetainedByteBudget)
        {
            m_Stats.RetainedBytes -= m_Retired.back().Key.Size;
            m_Retired.pop_back();
            m_Stats.RetiredTextures--;
        }
    }
} // namespace Crowny
