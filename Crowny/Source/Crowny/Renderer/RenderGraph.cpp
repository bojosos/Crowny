#include "cwpch.h"

#include "Crowny/Renderer/RenderGraph.h"
#include "Crowny/Renderer/RenderGraphResources.h"
#include "Crowny/Utils/PixelUtils.h"

#include <algorithm>
#include <chrono>

namespace Crowny
{
    namespace
    {
        bool IsReadState(RenderGraphResourceState state)
        {
            switch (state)
            {
            case RenderGraphResourceState::ShaderRead:
            case RenderGraphResourceState::DepthRead:
            case RenderGraphResourceState::TransferRead:
            case RenderGraphResourceState::IndirectArgument:
            case RenderGraphResourceState::VertexBuffer:
            case RenderGraphResourceState::IndexBuffer:
            case RenderGraphResourceState::Present:
                return true;
            case RenderGraphResourceState::ShaderReadWrite:
            case RenderGraphResourceState::ColorAttachmentReadWrite:
                return true;
            default:
                return false;
            }
        }

        bool IsWriteState(RenderGraphResourceState state)
        {
            switch (state)
            {
            case RenderGraphResourceState::ShaderWrite:
            case RenderGraphResourceState::ShaderReadWrite:
            case RenderGraphResourceState::ColorAttachment:
            case RenderGraphResourceState::ColorAttachmentReadWrite:
            case RenderGraphResourceState::DepthWrite:
            case RenderGraphResourceState::TransferWrite:
                return true;
            default:
                return false;
            }
        }

        bool ResourcesCanAlias(const RenderGraphResourceDesc& first, const RenderGraphResourceDesc& second)
        {
            if (first.Type != second.Type || first.Lifetime != RenderGraphResourceLifetime::Transient ||
                second.Lifetime != RenderGraphResourceLifetime::Transient)
                return false;

            if (first.Type == RenderGraphResourceType::Texture)
                return first.Texture == second.Texture;

            return first.Buffer.Type == second.Buffer.Type && first.Buffer.Stride == second.Buffer.Stride && first.Buffer.Size == second.Buffer.Size;
        }

        uint64_t EstimateTextureBytes(const RenderGraphTextureDesc& desc)
        {
            uint64_t bytes = 0;
            const uint32_t mipCount = std::max(desc.MipLevels, 1u);
            for (uint32_t mip = 0; mip < mipCount; mip++)
            {
                const uint32_t width = std::max(desc.Width >> mip, 1u);
                const uint32_t height = std::max(desc.Height >> mip, 1u);
                const uint32_t depth = std::max(desc.Depth >> mip, 1u);
                bytes += PixelUtils::GetMemorySize(width, height, depth, desc.Format);
            }
            return bytes * std::max(desc.Layers, 1u) * std::max(desc.Samples, 1u);
        }
    } // namespace

    const RenderGraphResourceInfo& RenderGraphContext::GetResource(RenderGraphResourceHandle handle) const
    {
        CW_ENGINE_ASSERT(handle.IsValid() && handle.Index < m_CompiledGraph.Resources.size(), "Invalid render graph resource handle");
        const RenderGraphResourceInfo& resource = m_CompiledGraph.Resources[handle.Index];
        CW_ENGINE_ASSERT(resource.Handle.Generation == handle.Generation && resource.Handle.Type == handle.Type,
                         "Stale render graph resource handle");
        return resource;
    }

    const RenderGraphResourceBinding& RenderGraphContext::GetBinding(RenderGraphResourceHandle handle) const
    {
        CW_ENGINE_ASSERT(m_Resources != nullptr, "This render graph execution has no physical resource registry");
        return m_Resources->Get(handle);
    }

    const Ref<Texture>& RenderGraphContext::GetTexture(RenderGraphResourceHandle handle) const
    {
        CW_ENGINE_ASSERT(m_Resources != nullptr, "This render graph execution has no physical resource registry");
        return m_Resources->GetTexture(handle);
    }

    const Ref<RenderTarget>& RenderGraphContext::GetRenderTarget(RenderGraphResourceHandle handle) const
    {
        static const Ref<RenderTarget> empty;
        if (m_Resources == nullptr)
            return empty;
        return m_Resources->GetRenderTarget(handle);
    }

    Ref<RenderTarget> RenderGraphContext::GetRenderTarget(const RenderGraphRenderTargetDesc& desc) const
    {
        return m_Resources != nullptr ? m_Resources->GetRenderTarget(desc) : nullptr;
    }

    const Ref<GenericGpuBuffer>& RenderGraphContext::GetBuffer(RenderGraphResourceHandle handle) const
    {
        CW_ENGINE_ASSERT(m_Resources != nullptr, "This render graph execution has no physical resource registry");
        return m_Resources->GetBuffer(handle);
    }

    bool RenderGraphContext::IsHistoryValid(RenderGraphResourceHandle handle) const
    {
        return m_Resources != nullptr && m_Resources->Get(handle).HistoryValid;
    }

    void RenderGraphPassBuilder::Read(RenderGraphResourceHandle resource, RenderGraphResourceState state)
    {
        m_Graph.AddUse(m_Pass, resource, state, RenderGraph::Access::Read);
    }

    void RenderGraphPassBuilder::Write(RenderGraphResourceHandle resource, RenderGraphResourceState state)
    {
        m_Graph.AddUse(m_Pass, resource, state, RenderGraph::Access::Write);
    }

    void RenderGraphPassBuilder::ReadWrite(RenderGraphResourceHandle resource, RenderGraphResourceState state)
    {
        m_Graph.AddUse(m_Pass, resource, state, RenderGraph::Access::ReadWrite);
    }

    void RenderGraphPassBuilder::DependsOn(RenderGraphPassHandle dependency) { m_Graph.AddDependency(m_Pass, dependency); }

    void RenderGraphPassBuilder::SetSideEffect(bool sideEffect) { m_Graph.SetSideEffect(m_Pass, sideEffect); }

    RenderGraphResourceHandle RenderGraph::CreateTexture(StringView name, const RenderGraphTextureDesc& desc, RenderGraphResourceLifetime lifetime)
    {
        RenderGraphResourceHandle handle{ static_cast<uint32_t>(m_Resources.Size()), m_Generation, RenderGraphResourceType::Texture };
        ResourceNode& node = m_Resources.Acquire();
        node.Info.Handle = handle;
        node.Info.Name.assign(name.data(), name.size());
        node.Info.Desc.Type = RenderGraphResourceType::Texture;
        node.Info.Desc.Lifetime = lifetime;
        node.Info.Desc.Texture = desc;
        node.Info.Desc.Buffer = {};
        node.Info.Desc.InitialState = RenderGraphResourceState::Undefined;
        node.Info.Desc.FinalState = RenderGraphResourceState::Undefined;
        node.Info.Desc.ExternalId = 0;
        node.Info.Desc.HistoryId = 0;
        node.Info.Desc.HistoryRole = RenderGraphHistoryRole::Single;
        node.Info.FirstUse = RenderGraphPassHandle::InvalidIndex;
        node.Info.LastUse = RenderGraphPassHandle::InvalidIndex;
        node.Info.PhysicalIndex = RenderGraphResourceHandle::InvalidIndex;
        if (lifetime == RenderGraphResourceLifetime::History)
            node.Info.Desc.HistoryId = BuildHistoryId(name, RenderGraphResourceType::Texture);
        m_Dirty = true;
        return handle;
    }

    RenderGraphResourceHandle RenderGraph::CreateBuffer(StringView name, const RenderGraphBufferDesc& desc, RenderGraphResourceLifetime lifetime)
    {
        RenderGraphResourceHandle handle{ static_cast<uint32_t>(m_Resources.Size()), m_Generation, RenderGraphResourceType::Buffer };
        ResourceNode& node = m_Resources.Acquire();
        node.Info.Handle = handle;
        node.Info.Name.assign(name.data(), name.size());
        node.Info.Desc.Type = RenderGraphResourceType::Buffer;
        node.Info.Desc.Lifetime = lifetime;
        node.Info.Desc.Buffer = desc;
        node.Info.Desc.Texture = {};
        node.Info.Desc.InitialState = RenderGraphResourceState::Undefined;
        node.Info.Desc.FinalState = RenderGraphResourceState::Undefined;
        node.Info.Desc.ExternalId = 0;
        node.Info.Desc.HistoryId = 0;
        node.Info.Desc.HistoryRole = RenderGraphHistoryRole::Single;
        node.Info.FirstUse = RenderGraphPassHandle::InvalidIndex;
        node.Info.LastUse = RenderGraphPassHandle::InvalidIndex;
        node.Info.PhysicalIndex = RenderGraphResourceHandle::InvalidIndex;
        if (lifetime == RenderGraphResourceLifetime::History)
            node.Info.Desc.HistoryId = BuildHistoryId(name, RenderGraphResourceType::Buffer);
        m_Dirty = true;
        return handle;
    }

    RenderGraphHistoryPair RenderGraph::CreateHistoryTexture(StringView name, const RenderGraphTextureDesc& desc)
    {
        RenderGraphHistoryPair pair;
        pair.Read = CreateTexture(name, desc, RenderGraphResourceLifetime::History);
        m_Resources[pair.Read.Index].Info.Name += "Read";
        pair.Write = CreateTexture(name, desc, RenderGraphResourceLifetime::History);
        m_Resources[pair.Write.Index].Info.Name += "Write";
        const uint64_t historyId = BuildHistoryId(name, RenderGraphResourceType::Texture);
        m_Resources[pair.Read.Index].Info.Desc.HistoryId = historyId;
        m_Resources[pair.Read.Index].Info.Desc.HistoryRole = RenderGraphHistoryRole::Read;
        m_Resources[pair.Write.Index].Info.Desc.HistoryId = historyId;
        m_Resources[pair.Write.Index].Info.Desc.HistoryRole = RenderGraphHistoryRole::Write;
        return pair;
    }

    RenderGraphHistoryPair RenderGraph::CreateHistoryBuffer(StringView name, const RenderGraphBufferDesc& desc)
    {
        RenderGraphHistoryPair pair;
        pair.Read = CreateBuffer(name, desc, RenderGraphResourceLifetime::History);
        m_Resources[pair.Read.Index].Info.Name += "Read";
        pair.Write = CreateBuffer(name, desc, RenderGraphResourceLifetime::History);
        m_Resources[pair.Write.Index].Info.Name += "Write";
        const uint64_t historyId = BuildHistoryId(name, RenderGraphResourceType::Buffer);
        m_Resources[pair.Read.Index].Info.Desc.HistoryId = historyId;
        m_Resources[pair.Read.Index].Info.Desc.HistoryRole = RenderGraphHistoryRole::Read;
        m_Resources[pair.Write.Index].Info.Desc.HistoryId = historyId;
        m_Resources[pair.Write.Index].Info.Desc.HistoryRole = RenderGraphHistoryRole::Write;
        return pair;
    }

    RenderGraphResourceHandle RenderGraph::ImportTexture(StringView name, const RenderGraphTextureDesc& desc, uint64_t externalId,
                                                         RenderGraphResourceState initialState, RenderGraphResourceState finalState)
    {
        RenderGraphResourceHandle handle = CreateTexture(name, desc, RenderGraphResourceLifetime::External);
        ResourceNode& node = m_Resources[handle.Index];
        node.Info.Desc.ExternalId = externalId;
        node.Info.Desc.InitialState = initialState;
        node.Info.Desc.FinalState = finalState;
        return handle;
    }

    RenderGraphResourceHandle RenderGraph::ImportBuffer(StringView name, const RenderGraphBufferDesc& desc, uint64_t externalId,
                                                        RenderGraphResourceState initialState, RenderGraphResourceState finalState)
    {
        RenderGraphResourceHandle handle = CreateBuffer(name, desc, RenderGraphResourceLifetime::External);
        ResourceNode& node = m_Resources[handle.Index];
        node.Info.Desc.ExternalId = externalId;
        node.Info.Desc.InitialState = initialState;
        node.Info.Desc.FinalState = finalState;
        return handle;
    }

    RenderGraphPassHandle RenderGraph::AddPass(StringView name, RenderGraphQueue queue, const SetupCallback& setup, ExecuteCallback execute)
    {
        RenderGraphPassHandle handle{ static_cast<uint32_t>(m_Passes.Size()), m_Generation };
        PassNode& node = m_Passes.Acquire();
        node.Handle = handle;
        node.Name.assign(name.data(), name.size());
        node.Queue = queue;
        node.Uses.clear();
        node.Dependencies.clear();
        node.Execute = std::move(execute);
        node.SideEffect = false;

        if (setup)
        {
            RenderGraphPassBuilder builder(*this, handle);
            setup(builder);
        }

        m_Dirty = true;
        return handle;
    }

    void RenderGraph::AddDependency(RenderGraphPassHandle pass, RenderGraphPassHandle dependency)
    {
        CW_ENGINE_ASSERT(ValidateHandle(pass), "Invalid render graph pass handle");
        CW_ENGINE_ASSERT(ValidateHandle(dependency), "Invalid render graph dependency handle");
        if (!ValidateHandle(pass) || !ValidateHandle(dependency))
            return;

        Vector<RenderGraphPassHandle>& dependencies = m_Passes[pass.Index].Dependencies;
        if (std::find(dependencies.begin(), dependencies.end(), dependency) == dependencies.end())
            dependencies.push_back(dependency);
        m_Dirty = true;
    }

    void RenderGraph::AddUse(RenderGraphPassHandle pass, RenderGraphResourceHandle resource, RenderGraphResourceState state, Access access)
    {
        CW_ENGINE_ASSERT(ValidateHandle(pass), "Invalid render graph pass handle");
        CW_ENGINE_ASSERT(ValidateHandle(resource), "Invalid render graph resource handle");
        if (!ValidateHandle(pass) || !ValidateHandle(resource))
            return;

        CW_ENGINE_ASSERT((IsRead(access) && IsReadState(state)) || (IsWrite(access) && IsWriteState(state)),
                         "Render graph resource state does not match access");

        PassNode& node = m_Passes[pass.Index];
        const auto existing = std::find_if(node.Uses.begin(), node.Uses.end(), [&](const ResourceUse& use) { return use.Resource == resource; });
        CW_ENGINE_ASSERT(existing == node.Uses.end(), "A render graph pass may declare each resource only once");
        if (existing != node.Uses.end())
            return;

        node.Uses.push_back({ resource, state, access });
        m_Dirty = true;
    }

    void RenderGraph::SetSideEffect(RenderGraphPassHandle pass, bool sideEffect)
    {
        CW_ENGINE_ASSERT(ValidateHandle(pass), "Invalid render graph pass handle");
        if (ValidateHandle(pass))
        {
            m_Passes[pass.Index].SideEffect = sideEffect;
            m_Dirty = true;
        }
    }

    bool RenderGraph::ValidateHandle(RenderGraphResourceHandle resource) const
    {
        return resource.IsValid() && resource.Generation == m_Generation && resource.Index < m_Resources.Size() &&
               m_Resources[resource.Index].Info.Handle.Type == resource.Type;
    }

    bool RenderGraph::ValidateHandle(RenderGraphPassHandle pass) const
    {
        return pass.IsValid() && pass.Generation == m_Generation && pass.Index < m_Passes.Size();
    }

    bool RenderGraph::IsWrite(Access access) { return access == Access::Write || access == Access::ReadWrite; }

    bool RenderGraph::IsRead(Access access) { return access == Access::Read || access == Access::ReadWrite; }

    uint64_t RenderGraph::BuildHistoryId(StringView name, RenderGraphResourceType type)
    {
        // Stable FNV-1a keeps history identities independent of per-frame graph handles.
        uint64_t hash = 14695981039346656037ull;
        for (const char character : name)
        {
            hash ^= static_cast<uint8_t>(character);
            hash *= 1099511628211ull;
        }
        hash ^= static_cast<uint8_t>(type);
        hash *= 1099511628211ull;
        return hash == 0 ? 1 : hash;
    }

    void RenderGraph::ResetCompileResult()
    {
        m_CompileScratch.ResourceRecycle.Reset();
        for (RenderGraphResourceInfo& resource : m_CompileResult.Resources)
            m_CompileScratch.ResourceRecycle.Acquire() = std::move(resource);

        m_CompileResult.Succeeded = false;
        m_CompileResult.Error.clear();
        m_CompileResult.PassOrder.clear();
        m_CompileResult.Barriers.clear();
        m_CompileResult.Resources.clear();
        m_CompileScratch.ResourceRecycle.Reset();
        m_CompileResult.PhysicalTextureCount = 0;
        m_CompileResult.PhysicalBufferCount = 0;
        m_CompileResult.TransientTextureBytes = 0;
        m_CompileResult.TransientBufferBytes = 0;
    }

    const RenderGraphCompileResult& RenderGraph::Compile()
    {
        if (!m_Dirty)
            return m_CompileResult;

        ResetCompileResult();
        for (const ResourceNode& resource : m_Resources)
        {
            RenderGraphResourceInfo& compiled = m_CompileScratch.ResourceRecycle.Acquire();
            compiled.Handle = resource.Info.Handle;
            compiled.Name = resource.Info.Name;
            compiled.Desc = resource.Info.Desc;
            compiled.FirstUse = RenderGraphPassHandle::InvalidIndex;
            compiled.LastUse = RenderGraphPassHandle::InvalidIndex;
            compiled.PhysicalIndex = RenderGraphResourceHandle::InvalidIndex;
            m_CompileResult.Resources.push_back(std::move(compiled));
        }

        const uint32_t passCount = static_cast<uint32_t>(m_Passes.Size());
        m_CompileScratch.Dependencies.Reset();
        m_CompileScratch.Dependents.Reset();
        for (uint32_t pass = 0; pass < passCount; pass++)
        {
            m_CompileScratch.Dependencies.Acquire().clear();
            m_CompileScratch.Dependents.Acquire().clear();
        }
        FrameVector<Vector<uint32_t>>& dependencies = m_CompileScratch.Dependencies;
        FrameVector<Vector<uint32_t>>& dependents = m_CompileScratch.Dependents;

        auto addDependency = [&](uint32_t pass, uint32_t dependency) {
            if (pass == dependency)
                return;
            Vector<uint32_t>& passDependencies = dependencies[pass];
            if (std::find(passDependencies.begin(), passDependencies.end(), dependency) == passDependencies.end())
            {
                passDependencies.push_back(dependency);
                dependents[dependency].push_back(pass);
            }
        };

        for (const PassNode& pass : m_Passes)
        {
            for (RenderGraphPassHandle dependency : pass.Dependencies)
            {
                if (!ValidateHandle(dependency))
                {
                    m_CompileResult.Error = "Pass '" + pass.Name + "' has an invalid explicit dependency";
                    m_Dirty = false;
                    return m_CompileResult;
                }
                addDependency(pass.Handle.Index, dependency.Index);
            }
        }

        m_CompileScratch.Hazards.Reset();
        for (uint32_t resource = 0; resource < m_Resources.Size(); resource++)
        {
            HazardState& hazard = m_CompileScratch.Hazards.Acquire();
            hazard.LastWriter = RenderGraphPassHandle::InvalidIndex;
            hazard.Readers.clear();
            hazard.Initialized = false;
        }
        FrameVector<HazardState>& hazards = m_CompileScratch.Hazards;

        for (const PassNode& pass : m_Passes)
        {
            for (const ResourceUse& use : pass.Uses)
            {
                HazardState& hazard = hazards[use.Resource.Index];
                const RenderGraphResourceInfo& resource = m_Resources[use.Resource.Index].Info;

                if (IsRead(use.ResourceAccess) && !hazard.Initialized && resource.Desc.Lifetime == RenderGraphResourceLifetime::Transient)
                {
                    m_CompileResult.Error = "Pass '" + pass.Name + "' reads transient resource '" + resource.Name + "' before its first write";
                    m_Dirty = false;
                    return m_CompileResult;
                }

                if (IsRead(use.ResourceAccess) && hazard.LastWriter != RenderGraphPassHandle::InvalidIndex)
                    addDependency(pass.Handle.Index, hazard.LastWriter);

                if (IsWrite(use.ResourceAccess))
                {
                    if (hazard.LastWriter != RenderGraphPassHandle::InvalidIndex)
                        addDependency(pass.Handle.Index, hazard.LastWriter);
                    for (uint32_t reader : hazard.Readers)
                        addDependency(pass.Handle.Index, reader);
                    hazard.Readers.clear();
                    hazard.LastWriter = pass.Handle.Index;
                    hazard.Initialized = true;
                }
                else
                {
                    if (std::find(hazard.Readers.begin(), hazard.Readers.end(), pass.Handle.Index) == hazard.Readers.end())
                        hazard.Readers.push_back(pass.Handle.Index);
                }
            }
        }

        Vector<uint32_t>& indegree = m_CompileScratch.Indegree;
        indegree.assign(passCount, 0);
        Vector<uint32_t>& ready = m_CompileScratch.Ready;
        ready.clear();
        const std::greater<uint32_t> readyOrder;
        for (uint32_t pass = 0; pass < passCount; pass++)
        {
            indegree[pass] = static_cast<uint32_t>(dependencies[pass].size());
            if (indegree[pass] == 0)
            {
                ready.push_back(pass);
                std::push_heap(ready.begin(), ready.end(), readyOrder);
            }
        }

        while (!ready.empty())
        {
            std::pop_heap(ready.begin(), ready.end(), readyOrder);
            const uint32_t pass = ready.back();
            ready.pop_back();
            m_CompileResult.PassOrder.push_back(m_Passes[pass].Handle);
            for (uint32_t dependent : dependents[pass])
            {
                if (--indegree[dependent] == 0)
                {
                    ready.push_back(dependent);
                    std::push_heap(ready.begin(), ready.end(), readyOrder);
                }
            }
        }

        if (m_CompileResult.PassOrder.size() != passCount)
        {
            m_CompileResult.Error = "Render graph contains a dependency cycle";
            m_CompileResult.PassOrder.clear();
            m_Dirty = false;
            return m_CompileResult;
        }

        Vector<uint32_t>& orderPosition = m_CompileScratch.OrderPosition;
        orderPosition.resize(passCount);
        for (uint32_t position = 0; position < m_CompileResult.PassOrder.size(); position++)
            orderPosition[m_CompileResult.PassOrder[position].Index] = position;

        for (RenderGraphResourceInfo& resource : m_CompileResult.Resources)
        {
            for (const PassNode& pass : m_Passes)
            {
                const bool used =
                  std::any_of(pass.Uses.begin(), pass.Uses.end(), [&](const ResourceUse& use) { return use.Resource == resource.Handle; });
                if (!used)
                    continue;
                const uint32_t position = orderPosition[pass.Handle.Index];
                resource.FirstUse = std::min(resource.FirstUse, position);
                resource.LastUse = resource.LastUse == RenderGraphPassHandle::InvalidIndex ? position : std::max(resource.LastUse, position);
            }
        }

        Vector<PhysicalAllocation>& textureAllocations = m_CompileScratch.TextureAllocations;
        Vector<PhysicalAllocation>& bufferAllocations = m_CompileScratch.BufferAllocations;
        textureAllocations.clear();
        bufferAllocations.clear();

        Vector<uint32_t>& resourcesByFirstUse = m_CompileScratch.ResourcesByFirstUse;
        resourcesByFirstUse.resize(m_CompileResult.Resources.size());
        for (uint32_t index = 0; index < resourcesByFirstUse.size(); index++)
            resourcesByFirstUse[index] = index;
        std::sort(resourcesByFirstUse.begin(), resourcesByFirstUse.end(), [&](uint32_t first, uint32_t second) {
            const uint32_t firstUse = m_CompileResult.Resources[first].FirstUse;
            const uint32_t secondUse = m_CompileResult.Resources[second].FirstUse;
            return firstUse != secondUse ? firstUse < secondUse : first < second;
        });

        for (uint32_t resourceIndex : resourcesByFirstUse)
        {
            RenderGraphResourceInfo& resource = m_CompileResult.Resources[resourceIndex];
            if (resource.FirstUse == RenderGraphPassHandle::InvalidIndex)
                continue;

            Vector<PhysicalAllocation>& allocations = resource.Desc.Type == RenderGraphResourceType::Texture ? textureAllocations : bufferAllocations;
            if (resource.Desc.Lifetime == RenderGraphResourceLifetime::Transient)
            {
                const auto allocation = std::find_if(allocations.begin(), allocations.end(), [&](const PhysicalAllocation& candidate) {
                    return candidate.LastUse < resource.FirstUse && ResourcesCanAlias(candidate.Desc, resource.Desc);
                });
                if (allocation != allocations.end())
                {
                    resource.PhysicalIndex = allocation->Index;
                    allocation->LastUse = resource.LastUse;
                    continue;
                }
            }

            resource.PhysicalIndex = static_cast<uint32_t>(allocations.size());
            allocations.push_back({ resource.Desc, resource.LastUse, resource.PhysicalIndex });
        }

        m_CompileResult.PhysicalTextureCount = static_cast<uint32_t>(textureAllocations.size());
        m_CompileResult.PhysicalBufferCount = static_cast<uint32_t>(bufferAllocations.size());
        for (const PhysicalAllocation& allocation : textureAllocations)
            if (allocation.Desc.Lifetime == RenderGraphResourceLifetime::Transient)
                m_CompileResult.TransientTextureBytes += EstimateTextureBytes(allocation.Desc.Texture);
        for (const PhysicalAllocation& allocation : bufferAllocations)
            if (allocation.Desc.Lifetime == RenderGraphResourceLifetime::Transient)
                m_CompileResult.TransientBufferBytes += allocation.Desc.Buffer.Size;

        Vector<LastUseState>& lastStates = m_CompileScratch.LastStates;
        lastStates.assign(m_Resources.Size(), {});
        for (uint32_t resourceIndex = 0; resourceIndex < m_Resources.Size(); resourceIndex++)
        {
            const RenderGraphResourceDesc& desc = m_Resources[resourceIndex].Info.Desc;
            if (desc.Lifetime != RenderGraphResourceLifetime::Transient && desc.InitialState != RenderGraphResourceState::Undefined)
                lastStates[resourceIndex] = { true, desc.InitialState, RenderGraphQueue::Graphics, Access::Read };
        }

        for (RenderGraphPassHandle passHandle : m_CompileResult.PassOrder)
        {
            const PassNode& pass = m_Passes[passHandle.Index];
            for (const ResourceUse& use : pass.Uses)
            {
                LastUseState& last = lastStates[use.Resource.Index];
                const bool stateChange = !last.Valid || last.State != use.State || last.Queue != pass.Queue;
                const bool memoryHazard = last.Valid && (IsWrite(last.ResourceAccess) || IsWrite(use.ResourceAccess));
                if (stateChange || memoryHazard)
                {
                    m_CompileResult.Barriers.push_back({ use.Resource, pass.Handle, last.Valid ? last.State : RenderGraphResourceState::Undefined,
                                                         use.State, last.Valid ? last.Queue : pass.Queue, pass.Queue });
                }
                last = { true, use.State, pass.Queue, use.ResourceAccess };
            }
        }

        for (uint32_t resourceIndex = 0; resourceIndex < m_Resources.Size(); resourceIndex++)
        {
            const RenderGraphResourceInfo& resource = m_Resources[resourceIndex].Info;
            if (resource.Desc.Lifetime == RenderGraphResourceLifetime::Transient || resource.Desc.FinalState == RenderGraphResourceState::Undefined ||
                !lastStates[resourceIndex].Valid)
                continue;

            const LastUseState& last = lastStates[resourceIndex];
            if (last.State != resource.Desc.FinalState)
            {
                const RenderGraphResourceInfo& compiledResource = m_CompileResult.Resources[resourceIndex];
                const RenderGraphPassHandle afterLastUse = m_CompileResult.PassOrder[compiledResource.LastUse];
                m_CompileResult.Barriers.push_back(
                  { resource.Handle, afterLastUse, last.State, resource.Desc.FinalState, last.Queue, last.Queue, true });
            }
        }

        m_CompileResult.Succeeded = true;
        m_Dirty = false;
        return m_CompileResult;
    }

    bool RenderGraph::Execute(IRenderGraphExecutionListener* listener, RenderGraphResourceRegistry* resources)
    {
        m_ExecutionStats = {};
        const auto executionStart = std::chrono::steady_clock::now();
        const RenderGraphCompileResult& compiled = Compile();
        if (!compiled.Succeeded)
            return false;
        m_ExecutionStats.ScheduledBarriers = static_cast<uint32_t>(compiled.Barriers.size());

        if (listener != nullptr)
            listener->BeginGraph(compiled);
        for (RenderGraphPassHandle passHandle : compiled.PassOrder)
        {
            PassNode& pass = m_Passes[passHandle.Index];
            if (listener != nullptr)
            {
                for (const RenderGraphBarrier& barrier : compiled.Barriers)
                    if (barrier.BeforePass == passHandle && !barrier.AfterPass)
                    {
                        listener->ApplyBarrier(barrier);
                        m_ExecutionStats.AppliedBarriers++;
                    }
                listener->BeginPass(passHandle, pass.Name, pass.Queue);
            }
            if (pass.Execute)
            {
                RenderGraphContext context(passHandle, compiled, resources);
                pass.Execute(context);
                m_ExecutionStats.ExecutedCallbacks++;
            }
            m_ExecutionStats.ExecutedPasses++;
            switch (pass.Queue)
            {
            case RenderGraphQueue::Graphics:
                m_ExecutionStats.GraphicsPasses++;
                break;
            case RenderGraphQueue::Compute:
                m_ExecutionStats.ComputePasses++;
                break;
            case RenderGraphQueue::Transfer:
                m_ExecutionStats.TransferPasses++;
                break;
            }
            if (listener != nullptr)
            {
                listener->EndPass(passHandle, pass.Name, pass.Queue);
                for (const RenderGraphBarrier& barrier : compiled.Barriers)
                    if (barrier.BeforePass == passHandle && barrier.AfterPass)
                    {
                        listener->ApplyBarrier(barrier);
                        m_ExecutionStats.AppliedBarriers++;
                    }
            }
            if (resources != nullptr && resources->HasAllocationFailure())
            {
                if (listener != nullptr)
                    listener->EndGraph(compiled);
                m_ExecutionStats.CpuTimeMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - executionStart).count();
                return false;
            }
        }
        if (listener != nullptr)
            listener->EndGraph(compiled);
        m_ExecutionStats.CpuTimeMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - executionStart).count();
        m_ExecutionStats.Succeeded = true;
        return true;
    }

    void RenderGraph::Reset()
    {
        for (PassNode& pass : m_Passes)
            pass.Execute = {};
        m_Resources.Reset();
        m_Passes.Reset();
        ResetCompileResult();
        m_ExecutionStats = {};
        m_Generation++;
        if (m_Generation == 0)
            m_Generation = 1;
        m_Dirty = true;
    }

    const String& RenderGraph::GetPassName(RenderGraphPassHandle pass) const
    {
        static const String empty;
        return ValidateHandle(pass) ? m_Passes[pass.Index].Name : empty;
    }

} // namespace Crowny
