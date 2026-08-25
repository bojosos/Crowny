#pragma once

#include "Crowny/Common/StdHeaders.h"
#include "Crowny/Common/Types.h"
#include "Crowny/Memory/FrameVector.h"

#include <cstdint>
#include <functional>
#include <limits>

namespace Crowny
{

    enum class RenderGraphQueue : uint8_t
    {
        Graphics,
        Compute,
        Transfer
    };

    enum class RenderGraphResourceType : uint8_t
    {
        Texture,
        Buffer
    };

    enum class RenderGraphResourceLifetime : uint8_t
    {
        Transient,
        External,
        History
    };

    enum class RenderGraphResourceState : uint8_t
    {
        Undefined,
        ShaderRead,
        ShaderWrite,
        ShaderReadWrite,
        ColorAttachment,
        ColorAttachmentReadWrite,
        DepthRead,
        DepthWrite,
        TransferRead,
        TransferWrite,
        IndirectArgument,
        VertexBuffer,
        IndexBuffer,
        Present
    };

    enum class RenderGraphHistoryRole : uint8_t
    {
        Single,
        Read,
        Write
    };

    struct RenderGraphResourceHandle
    {
        static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

        uint32_t Index = InvalidIndex;
        uint32_t Generation = 0;
        RenderGraphResourceType Type = RenderGraphResourceType::Texture;

        bool IsValid() const { return Index != InvalidIndex; }
        explicit operator bool() const { return IsValid(); }
        bool operator==(const RenderGraphResourceHandle& other) const = default;
    };

    struct RenderGraphPassHandle
    {
        static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

        uint32_t Index = InvalidIndex;
        uint32_t Generation = 0;

        bool IsValid() const { return Index != InvalidIndex; }
        explicit operator bool() const { return IsValid(); }
        bool operator==(const RenderGraphPassHandle& other) const = default;
    };

    struct RenderGraphTextureDesc
    {
        TextureShape Shape = TextureShape::TEXTURE_2D;
        uint32_t Width = 1;
        uint32_t Height = 1;
        uint32_t Depth = 1;
        uint32_t MipLevels = 1;
        uint32_t Layers = 1;
        uint32_t Samples = 1;
        TextureFormat Format = TextureFormat::RGBA8;

        bool operator==(const RenderGraphTextureDesc& other) const = default;
    };

    struct RenderGraphBufferDesc
    {
        uint64_t Size = 0;
        uint32_t Stride = 0;
        GpuBufferType Type = GpuBufferType::Standard;

        bool operator==(const RenderGraphBufferDesc& other) const = default;
    };

    struct RenderGraphResourceDesc
    {
        RenderGraphResourceType Type = RenderGraphResourceType::Texture;
        RenderGraphResourceLifetime Lifetime = RenderGraphResourceLifetime::Transient;
        RenderGraphTextureDesc Texture;
        RenderGraphBufferDesc Buffer;
        RenderGraphResourceState InitialState = RenderGraphResourceState::Undefined;
        RenderGraphResourceState FinalState = RenderGraphResourceState::Undefined;
        uint64_t ExternalId = 0;
        uint64_t HistoryId = 0;
        RenderGraphHistoryRole HistoryRole = RenderGraphHistoryRole::Single;
    };

    struct RenderGraphHistoryPair
    {
        RenderGraphResourceHandle Read;
        RenderGraphResourceHandle Write;
    };

    struct RenderGraphBarrier
    {
        RenderGraphResourceHandle Resource;
        RenderGraphPassHandle BeforePass;
        RenderGraphResourceState SourceState = RenderGraphResourceState::Undefined;
        RenderGraphResourceState DestinationState = RenderGraphResourceState::Undefined;
        RenderGraphQueue SourceQueue = RenderGraphQueue::Graphics;
        RenderGraphQueue DestinationQueue = RenderGraphQueue::Graphics;
        bool AfterPass = false;
    };

    struct RenderGraphResourceInfo
    {
        RenderGraphResourceHandle Handle;
        String Name;
        RenderGraphResourceDesc Desc;
        uint32_t FirstUse = RenderGraphPassHandle::InvalidIndex;
        uint32_t LastUse = RenderGraphPassHandle::InvalidIndex;
        uint32_t PhysicalIndex = RenderGraphResourceHandle::InvalidIndex;
    };

    struct RenderGraphCompileResult
    {
        bool Succeeded = false;
        String Error;
        Vector<RenderGraphPassHandle> PassOrder;
        Vector<RenderGraphBarrier> Barriers;
        Vector<RenderGraphResourceInfo> Resources;
        uint32_t PhysicalTextureCount = 0;
        uint32_t PhysicalBufferCount = 0;
        uint64_t TransientTextureBytes = 0;
        uint64_t TransientBufferBytes = 0;
    };

    struct RenderGraphExecutionStats
    {
        uint32_t ExecutedPasses = 0;
        uint32_t ExecutedCallbacks = 0;
        uint32_t GraphicsPasses = 0;
        uint32_t ComputePasses = 0;
        uint32_t TransferPasses = 0;
        uint32_t ScheduledBarriers = 0;
        uint32_t AppliedBarriers = 0;
        double CpuTimeMs = 0.0;
        bool Succeeded = false;
    };

    class IRenderGraphExecutionListener
    {
    public:
        virtual ~IRenderGraphExecutionListener() = default;
        virtual void BeginGraph(const RenderGraphCompileResult&) {}
        virtual void ApplyBarrier(const RenderGraphBarrier&) {}
        virtual void BeginPass(RenderGraphPassHandle, StringView, RenderGraphQueue) {}
        virtual void EndPass(RenderGraphPassHandle, StringView, RenderGraphQueue) {}
        virtual void EndGraph(const RenderGraphCompileResult&) {}
    };

    class RenderGraph;
    class RenderGraphResourceRegistry;
    class GenericGpuBuffer;
    class RenderTarget;
    class Texture;
    struct RenderGraphResourceBinding;
    struct RenderGraphRenderTargetDesc;

    class RenderGraphContext
    {
    public:
        RenderGraphPassHandle GetPass() const { return m_Pass; }
        const RenderGraphCompileResult& GetCompiledGraph() const { return m_CompiledGraph; }
        const RenderGraphResourceInfo& GetResource(RenderGraphResourceHandle handle) const;
        const RenderGraphResourceBinding& GetBinding(RenderGraphResourceHandle handle) const;
        const Ref<Texture>& GetTexture(RenderGraphResourceHandle handle) const;
        const Ref<RenderTarget>& GetRenderTarget(RenderGraphResourceHandle handle) const;
        Ref<RenderTarget> GetRenderTarget(const RenderGraphRenderTargetDesc& desc) const;
        const Ref<GenericGpuBuffer>& GetBuffer(RenderGraphResourceHandle handle) const;
        bool IsHistoryValid(RenderGraphResourceHandle handle) const;

    private:
        friend class RenderGraph;
        RenderGraphContext(RenderGraphPassHandle pass, const RenderGraphCompileResult& compiledGraph, RenderGraphResourceRegistry* resources)
          : m_Pass(pass), m_CompiledGraph(compiledGraph), m_Resources(resources)
        {
        }

        RenderGraphPassHandle m_Pass;
        const RenderGraphCompileResult& m_CompiledGraph;
        RenderGraphResourceRegistry* m_Resources = nullptr;
    };

    class RenderGraphPassBuilder
    {
    public:
        void Read(RenderGraphResourceHandle resource, RenderGraphResourceState state = RenderGraphResourceState::ShaderRead);
        void Write(RenderGraphResourceHandle resource, RenderGraphResourceState state = RenderGraphResourceState::ShaderWrite);
        void ReadWrite(RenderGraphResourceHandle resource, RenderGraphResourceState state = RenderGraphResourceState::ShaderReadWrite);
        void DependsOn(RenderGraphPassHandle dependency);
        void SetSideEffect(bool sideEffect = true);

    private:
        friend class RenderGraph;
        RenderGraphPassBuilder(RenderGraph& graph, RenderGraphPassHandle pass) : m_Graph(graph), m_Pass(pass) {}

        RenderGraph& m_Graph;
        RenderGraphPassHandle m_Pass;
    };

    class RenderGraph
    {
    public:
        using SetupCallback = std::function<void(RenderGraphPassBuilder&)>;
        using ExecuteCallback = std::function<void(RenderGraphContext&)>;

        RenderGraphResourceHandle CreateTexture(StringView name, const RenderGraphTextureDesc& desc,
                                                RenderGraphResourceLifetime lifetime = RenderGraphResourceLifetime::Transient);
        RenderGraphResourceHandle CreateBuffer(StringView name, const RenderGraphBufferDesc& desc,
                                               RenderGraphResourceLifetime lifetime = RenderGraphResourceLifetime::Transient);
        RenderGraphHistoryPair CreateHistoryTexture(StringView name, const RenderGraphTextureDesc& desc);
        RenderGraphHistoryPair CreateHistoryBuffer(StringView name, const RenderGraphBufferDesc& desc);
        RenderGraphResourceHandle ImportTexture(StringView name, const RenderGraphTextureDesc& desc, uint64_t externalId,
                                                RenderGraphResourceState initialState, RenderGraphResourceState finalState);
        RenderGraphResourceHandle ImportBuffer(StringView name, const RenderGraphBufferDesc& desc, uint64_t externalId,
                                               RenderGraphResourceState initialState, RenderGraphResourceState finalState);

        RenderGraphPassHandle AddPass(StringView name, RenderGraphQueue queue, const SetupCallback& setup, ExecuteCallback execute = {});
        void AddDependency(RenderGraphPassHandle pass, RenderGraphPassHandle dependency);

        const RenderGraphCompileResult& Compile();
        bool Execute(IRenderGraphExecutionListener* listener = nullptr, RenderGraphResourceRegistry* resources = nullptr);
        void Reset();

        const RenderGraphCompileResult& GetCompileResult() const { return m_CompileResult; }
        const RenderGraphExecutionStats& GetExecutionStats() const { return m_ExecutionStats; }
        const String& GetPassName(RenderGraphPassHandle pass) const;

    private:
        friend class RenderGraphPassBuilder;

        enum class Access : uint8_t
        {
            Read,
            Write,
            ReadWrite
        };

        struct ResourceUse
        {
            RenderGraphResourceHandle Resource;
            RenderGraphResourceState State = RenderGraphResourceState::Undefined;
            Access ResourceAccess = Access::Read;
        };

        struct ResourceNode
        {
            RenderGraphResourceInfo Info;
        };

        struct PassNode
        {
            RenderGraphPassHandle Handle;
            String Name;
            RenderGraphQueue Queue = RenderGraphQueue::Graphics;
            Vector<ResourceUse> Uses;
            Vector<RenderGraphPassHandle> Dependencies;
            ExecuteCallback Execute;
            bool SideEffect = false;
        };

        struct HazardState
        {
            uint32_t LastWriter = RenderGraphPassHandle::InvalidIndex;
            Vector<uint32_t> Readers;
            bool Initialized = false;
        };

        struct PhysicalAllocation
        {
            RenderGraphResourceDesc Desc;
            uint32_t LastUse = 0;
            uint32_t Index = 0;
        };

        struct LastUseState
        {
            bool Valid = false;
            RenderGraphResourceState State = RenderGraphResourceState::Undefined;
            RenderGraphQueue Queue = RenderGraphQueue::Graphics;
            Access ResourceAccess = Access::Read;
        };

        struct CompileScratch
        {
            FrameVector<Vector<uint32_t>> Dependencies;
            FrameVector<Vector<uint32_t>> Dependents;
            FrameVector<HazardState> Hazards;
            Vector<uint32_t> Indegree;
            Vector<uint32_t> Ready;
            Vector<uint32_t> OrderPosition;
            Vector<PhysicalAllocation> TextureAllocations;
            Vector<PhysicalAllocation> BufferAllocations;
            Vector<uint32_t> ResourcesByFirstUse;
            Vector<LastUseState> LastStates;
            FrameVector<RenderGraphResourceInfo> ResourceRecycle;
        };

        void AddUse(RenderGraphPassHandle pass, RenderGraphResourceHandle resource, RenderGraphResourceState state, Access access);
        void SetSideEffect(RenderGraphPassHandle pass, bool sideEffect);
        void ResetCompileResult();
        bool ValidateHandle(RenderGraphResourceHandle resource) const;
        bool ValidateHandle(RenderGraphPassHandle pass) const;
        static bool IsWrite(Access access);
        static bool IsRead(Access access);
        static uint64_t BuildHistoryId(StringView name, RenderGraphResourceType type);

        FrameVector<ResourceNode> m_Resources;
        FrameVector<PassNode> m_Passes;
        RenderGraphCompileResult m_CompileResult;
        CompileScratch m_CompileScratch;
        RenderGraphExecutionStats m_ExecutionStats;
        uint32_t m_Generation = 1;
        bool m_Dirty = true;
    };

} // namespace Crowny
