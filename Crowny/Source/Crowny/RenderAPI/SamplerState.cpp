#include "cwpch.h"

#include "Crowny/RenderAPI/SamplerState.h"
#include "Crowny/Renderer/Renderer.h"

#include "Platform/OpenGL/OpenGLSamplerState.h"
#include "Platform/Vulkan/VulkanSamplerState.h"

#include <bit>

namespace Crowny
{
    namespace
    {
        struct SamplerKey
        {
            TextureFilter MinFilter;
            TextureFilter MagFilter;
            TextureFilter MipFilter;
            uint32_t MaxAnisotropy;
            uint32_t MipmapBias;
            uint32_t MipMin;
            uint32_t MipMax;
            TextureWrap AddressU;
            TextureWrap AddressV;
            TextureWrap AddressW;
            CompareFunction Compare;

            bool operator==(const SamplerKey& other) const = default;
        };

        struct SamplerKeyHash
        {
            size_t operator()(const SamplerKey& key) const
            {
                size_t hash = 1469598103934665603ull;
                const auto combine = [&](uint32_t value) {
                    hash ^= static_cast<size_t>(value);
                    hash *= 1099511628211ull;
                };
                combine(static_cast<uint32_t>(key.MinFilter));
                combine(static_cast<uint32_t>(key.MagFilter));
                combine(static_cast<uint32_t>(key.MipFilter));
                combine(key.MaxAnisotropy);
                combine(key.MipmapBias);
                combine(key.MipMin);
                combine(key.MipMax);
                combine(static_cast<uint32_t>(key.AddressU));
                combine(static_cast<uint32_t>(key.AddressV));
                combine(static_cast<uint32_t>(key.AddressW));
                combine(static_cast<uint32_t>(key.Compare));
                return hash;
            }
        };

        SamplerKey MakeKey(const SamplerStateDesc& desc)
        {
            return { desc.MinFilter,
                     desc.MagFilter,
                     desc.MipFilter,
                     desc.MaxAnsio,
                     std::bit_cast<uint32_t>(desc.MipmapBias),
                     std::bit_cast<uint32_t>(desc.MipMin),
                     std::bit_cast<uint32_t>(desc.MipMax),
                     desc.AddressMode.U,
                     desc.AddressMode.V,
                     desc.AddressMode.W,
                     desc.CompareFunc };
        }

        std::mutex s_SamplerCacheMutex;
        UnorderedMap<SamplerKey, Ref<SamplerState>, SamplerKeyHash> s_SamplerCache;
    } // namespace

    Ref<SamplerState> SamplerState::s_DefaultSamplerState = nullptr;

    SamplerState::SamplerState(const SamplerStateDesc& desc) : m_Properties(desc) {}

    const Ref<SamplerState>& SamplerState::GetDefault()
    {
        if (s_DefaultSamplerState == nullptr)
            s_DefaultSamplerState = SamplerState::Create({});
        return s_DefaultSamplerState;
    }

    Ref<SamplerState> SamplerState::Create(const SamplerStateDesc& desc)
    {
        const SamplerKey key = MakeKey(desc);
        std::scoped_lock lock(s_SamplerCacheMutex);
        const auto cached = s_SamplerCache.find(key);
        if (cached != s_SamplerCache.end())
            return cached->second;

        Ref<SamplerState> sampler;
        switch (RenderAPI::TryGet()->GetAPI())
        {
        case RenderAPI::API::OpenGL:
            sampler = Ref<SamplerState>(new OpenGLSamplerState(desc));
            break;
        case RenderAPI::API::Vulkan:
            sampler = Ref<SamplerState>(new VulkanSamplerState(desc));
            break;
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supported");
            return nullptr;
        }

        s_SamplerCache.emplace(key, sampler);
        return sampler;
    }

    void SamplerState::ClearCache()
    {
        std::scoped_lock lock(s_SamplerCacheMutex);
        s_SamplerCache.clear();
    }

} // namespace Crowny
