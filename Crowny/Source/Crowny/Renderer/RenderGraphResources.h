#pragma once

#include "Crowny/Common/Constants.h"
#include "Crowny/RenderAPI/GenericGpuBuffer.h"
#include "Crowny/RenderAPI/RenderTarget.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/GpuBufferPool.h"
#include "Crowny/Renderer/RenderGraph.h"

#include <array>

namespace Crowny
{
    class IRenderGraphResourceAllocator
    {
    public:
        virtual ~IRenderGraphResourceAllocator() = default;
        virtual void BeginFrame(uint64_t frameNumber) { static_cast<void>(frameNumber); }
        virtual Ref<Texture> CreateTexture(StringView name, const RenderGraphTextureDesc& desc) = 0;
        virtual Ref<GenericGpuBuffer> CreateBuffer(StringView name, const RenderGraphBufferDesc& desc) = 0;
        virtual void ReleaseBuffer(const RenderGraphBufferDesc& desc, Ref<GenericGpuBuffer>&& buffer)
        {
            static_cast<void>(desc);
            static_cast<void>(buffer);
        }
    };

    class RenderGraphGpuResourceAllocator final : public IRenderGraphResourceAllocator
    {
    public:
        explicit RenderGraphGpuResourceAllocator(uint32_t framesInFlight = 2,
                                                 uint64_t retainedBufferBudget = 64ull * 1024ull * 1024ull)
          : m_BufferPool(framesInFlight, retainedBufferBudget)
        {
        }

        void BeginFrame(uint64_t frameNumber) override;
        Ref<Texture> CreateTexture(StringView name, const RenderGraphTextureDesc& desc) override;
        Ref<GenericGpuBuffer> CreateBuffer(StringView name, const RenderGraphBufferDesc& desc) override;
        void ReleaseBuffer(const RenderGraphBufferDesc& desc, Ref<GenericGpuBuffer>&& buffer) override;
        GpuBufferPoolStats GetBufferPoolStats() const { return m_BufferPool.GetStats(); }

    private:
        GpuBufferPool m_BufferPool;
    };

    struct RenderGraphResourceBinding
    {
        uint64_t PhysicalId = 0;
        uint64_t ExternalId = 0;
        RenderGraphResourceType Type = RenderGraphResourceType::Texture;
        Ref<Texture> TextureResource;
        Ref<RenderTarget> RenderTargetResource;
        Ref<GenericGpuBuffer> BufferResource;
        bool AllocationAttempted = false;
        bool HistoryValid = false;
    };

    struct RenderGraphResourceRegistryStats
    {
        uint32_t TransientAllocations = 0;
        uint32_t HistoryAllocations = 0;
        uint32_t HistoryInvalidations = 0;
        uint32_t TextureAllocations = 0;
        uint32_t BufferAllocations = 0;
        uint32_t RenderTargetAllocations = 0;
        uint32_t RenderTargetCacheHits = 0;
        uint32_t AllocationFailures = 0;
    };

    struct RenderGraphRenderTargetDesc
    {
        std::array<RenderGraphResourceHandle, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS> Colors{};
        RenderGraphResourceHandle Depth;
        uint32_t ColorCount = 0;
        uint32_t MipLevel = 0;
        uint32_t FirstLayer = 0;
        uint32_t LayerCount = 1;
    };

    // Resolves logical graph handles to stable physical identities. Backend
    // allocators attach textures and buffers to these identities; this class
    // owns aliasing, frames-in-flight reuse, and camera-local history swaps.
    class RenderGraphResourceRegistry
    {
    public:
        explicit RenderGraphResourceRegistry(uint32_t framesInFlight = 2,
                                             IRenderGraphResourceAllocator* allocator = nullptr);

        bool BeginFrame(const RenderGraphCompileResult& graph, uint64_t frameNumber,
                        uint64_t historyNamespace, bool resetHistory = false);
        void EndFrame();
        void InvalidateHistory(uint64_t historyNamespace);
        void Reset();

        const RenderGraphResourceBinding& Get(RenderGraphResourceHandle handle);
        const Ref<Texture>& GetTexture(RenderGraphResourceHandle handle);
        const Ref<RenderTarget>& GetRenderTarget(RenderGraphResourceHandle handle);
        Ref<RenderTarget> GetRenderTarget(const RenderGraphRenderTargetDesc& desc);
        const Ref<GenericGpuBuffer>& GetBuffer(RenderGraphResourceHandle handle);
        bool BindExternalTexture(RenderGraphResourceHandle handle, const Ref<Texture>& texture);
        bool BindExternalRenderTarget(RenderGraphResourceHandle handle, const Ref<RenderTarget>& renderTarget);
        bool BindExternalBuffer(RenderGraphResourceHandle handle, const Ref<GenericGpuBuffer>& buffer);
        bool HasAllocationFailure() const { return m_FrameAllocationFailed; }
        const String& GetAllocationError() const { return m_AllocationError; }
        const RenderGraphResourceRegistryStats& GetStats() const { return m_Stats; }

    private:
        struct TransientSlot
        {
            RenderGraphResourceDesc Desc;
            uint64_t PhysicalId = 0;
            bool Initialized = false;
        };

        struct HistoryEntry
        {
            RenderGraphResourceDesc Desc;
            std::array<uint64_t, 2> PhysicalIds{};
            bool Valid = false;
            bool Paired = false;
        };

        struct PhysicalResource
        {
            RenderGraphResourceDesc Desc;
            String Name;
            Ref<Texture> TextureResource;
            Ref<GenericGpuBuffer> BufferResource;
            bool AllocationAttempted = false;
        };

        struct RenderTargetKey
        {
            std::array<uint64_t, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS + 1> PhysicalIds{};
            uint32_t ColorCount = 0;
            uint32_t MipLevel = 0;
            uint32_t FirstLayer = 0;
            uint32_t LayerCount = 1;
            uint32_t Width = 0;
            uint32_t Height = 0;
            uint32_t Samples = 1;

            bool operator==(const RenderTargetKey& other) const = default;
        };

        struct RenderTargetKeyHash
        {
            size_t operator()(const RenderTargetKey& key) const;
        };

        static bool DescriptorsMatch(const RenderGraphResourceDesc& first,
                                     const RenderGraphResourceDesc& second);
        uint64_t AllocatePhysicalId();
        void RegisterPhysicalResource(uint64_t physicalId, const RenderGraphResourceInfo& resource);
        void ReleasePhysicalResource(uint64_t physicalId);
        void ResolvePhysicalResource(RenderGraphResourceBinding& binding);

        uint32_t m_FramesInFlight = 2;
        IRenderGraphResourceAllocator* m_Allocator = nullptr;
        Vector<Vector<TransientSlot>> m_TransientFrames;
        UnorderedMap<uint64_t, UnorderedMap<uint64_t, HistoryEntry>> m_History;
        UnorderedMap<uint64_t, PhysicalResource> m_PhysicalResources;
        UnorderedMap<RenderTargetKey, Ref<RenderTarget>, RenderTargetKeyHash> m_RenderTargets;
        Vector<RenderGraphResourceBinding> m_Bindings;
        const RenderGraphCompileResult* m_CurrentGraph = nullptr;
        uint64_t m_CurrentFrame = 0;
        uint64_t m_CurrentHistoryNamespace = 0;
        uint64_t m_NextPhysicalId = 1;
        bool m_FrameAllocationFailed = false;
        String m_AllocationError;
        RenderGraphResourceRegistryStats m_Stats;
    };
} // namespace Crowny
