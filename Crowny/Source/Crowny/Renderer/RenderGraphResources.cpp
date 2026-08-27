#include "cwpch.h"

#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/Renderer/RenderGraphResources.h"

namespace Crowny
{
    namespace
    {
        bool IsDepthFormat(TextureFormat format)
        {
            return format == TextureFormat::DEPTH32F || format == TextureFormat::DEPTH24STENCIL8;
        }
    } // namespace

    void RenderGraphGpuResourceAllocator::BeginFrame(uint64_t frameNumber)
    {
        m_BufferPool.BeginFrame(frameNumber);
        m_TexturePool.BeginFrame(frameNumber);
    }

    TextureDesc RenderGraphGpuResourceAllocator::MakeTextureDesc(StringView name, const RenderGraphTextureDesc& desc)
    {
        const uint32_t layers = std::max(desc.Layers, 1u);
        TextureDesc textureDesc;
        textureDesc.Shape = desc.Shape;
        textureDesc.sRGB = false;
        textureDesc.Width = std::max(desc.Width, 1u);
        textureDesc.Height = std::max(desc.Height, 1u);
        textureDesc.Depth = std::max(desc.Depth, 1u);
        textureDesc.MipLevels = std::max(desc.MipLevels, 1u) - 1u;
        textureDesc.Samples = std::max(desc.Samples, 1u);
        textureDesc.Faces = layers;
        textureDesc.Format = desc.Format;
        textureDesc.DebugName = String(name);
        if (IsDepthFormat(desc.Format))
            textureDesc.Usage = TextureUsage::TEXTURE_DEPTHSTENCIL;
        else if (textureDesc.Samples > 1)
            textureDesc.Usage = TextureUsage::TEXTURE_RENDERTARGET;
        else
            textureDesc.Usage = static_cast<TextureUsage>(TextureUsage::TEXTURE_RENDERTARGET | TextureUsage::TEXTURE_LOADSTORE);
        return textureDesc;
    }

    Ref<Texture> RenderGraphGpuResourceAllocator::CreateTexture(StringView name, const RenderGraphTextureDesc& desc)
    {
        if (RenderAPI::TryGet() == nullptr)
            return nullptr;

        const uint32_t layers = std::max(desc.Layers, 1u);
        if (RenderAPI::TryGet()->GetAPI() == RenderAPI::API::OpenGL && layers > 1)
            return nullptr;

        return m_TexturePool.Acquire(MakeTextureDesc(name, desc));
    }

    void RenderGraphGpuResourceAllocator::ReleaseTexture(const RenderGraphTextureDesc& desc, Ref<Texture>&& texture)
    {
        static_cast<void>(desc);
        m_TexturePool.Release(std::move(texture));
    }

    Ref<GenericGpuBuffer> RenderGraphGpuResourceAllocator::CreateBuffer(StringView name, const RenderGraphBufferDesc& desc)
    {
        static_cast<void>(name);
        if (RenderAPI::TryGet() == nullptr || desc.Size == 0)
            return nullptr;

        const uint32_t elementSize = std::max(desc.Stride, 1u);
        const uint64_t elementCount = (desc.Size + elementSize - 1u) / elementSize;
        if (elementCount > std::numeric_limits<uint32_t>::max() || elementCount * elementSize > std::numeric_limits<uint32_t>::max())
            return nullptr;

        GenericGpuBufferDesc bufferDesc;
        bufferDesc.ElementCount = static_cast<uint32_t>(elementCount);
        bufferDesc.ElementSize = elementSize;
        bufferDesc.Type = desc.Type;
        bufferDesc.Usage = BufferUsage::BU_LOADSTORE;
        return m_BufferPool.Acquire(bufferDesc);
    }

    void RenderGraphGpuResourceAllocator::ReleaseBuffer(const RenderGraphBufferDesc& desc,
                                                        Ref<GenericGpuBuffer>&& buffer)
    {
        if (!buffer)
            return;

        GenericGpuBufferDesc bufferDesc;
        bufferDesc.ElementSize = std::max(desc.Stride, 1u);
        bufferDesc.ElementCount = static_cast<uint32_t>((desc.Size + bufferDesc.ElementSize - 1u) /
                                                        bufferDesc.ElementSize);
        bufferDesc.Type = desc.Type;
        bufferDesc.Usage = BufferUsage::BU_LOADSTORE;
        m_BufferPool.Release(bufferDesc, std::move(buffer));
    }

    RenderGraphResourceRegistry::RenderGraphResourceRegistry(uint32_t framesInFlight,
                                                             IRenderGraphResourceAllocator* allocator)
      : m_FramesInFlight(std::max(framesInFlight, 1u)), m_Allocator(allocator), m_TransientFrames(m_FramesInFlight)
    {
    }

    bool RenderGraphResourceRegistry::BeginFrame(const RenderGraphCompileResult& graph, uint64_t frameNumber,
                                                 uint64_t historyNamespace, bool resetHistory)
    {
        if (!graph.Succeeded || m_CurrentGraph != nullptr)
            return false;
        if (resetHistory)
            InvalidateHistory(historyNamespace);

        m_CurrentGraph = &graph;
        m_CurrentFrame = frameNumber;
        m_CurrentHistoryNamespace = historyNamespace;
        m_FrameAllocationFailed = false;
        m_AllocationError.clear();
        m_Bindings.assign(graph.Resources.size(), {});
        if (m_Allocator != nullptr)
            m_Allocator->BeginFrame(frameNumber);
        Vector<TransientSlot>& transientSlots = m_TransientFrames[frameNumber % m_FramesInFlight];
        const size_t transientSlotCount = static_cast<size_t>(graph.PhysicalTextureCount) + graph.PhysicalBufferCount;
        for (size_t slotIndex = transientSlotCount; slotIndex < transientSlots.size(); slotIndex++)
            ReleasePhysicalResource(transientSlots[slotIndex].PhysicalId);
        transientSlots.resize(transientSlotCount);
        m_FramePhysicalIds.clear();
        if (transientSlotCount > m_FramePhysicalScratchCapacity)
        {
            m_FramePhysicalIds.reserve(transientSlotCount);
            m_FramePhysicalScratchCapacity = transientSlotCount;
            m_Stats.FramePhysicalScratchGrowths++;
            m_Stats.FramePhysicalScratchCapacity = m_FramePhysicalScratchCapacity;
        }

        for (const RenderGraphResourceInfo& resource : graph.Resources)
        {
            if (resource.FirstUse == RenderGraphPassHandle::InvalidIndex)
                continue;
            RenderGraphResourceBinding& binding = m_Bindings[resource.Handle.Index];
            binding.Type = resource.Desc.Type;
            if (resource.Desc.Lifetime == RenderGraphResourceLifetime::External)
            {
                binding.PhysicalId = resource.Desc.ExternalId;
                binding.ExternalId = resource.Desc.ExternalId;
                continue;
            }

            if (resource.Desc.Lifetime == RenderGraphResourceLifetime::Transient)
            {
                const uint64_t frameKey = (static_cast<uint64_t>(resource.Desc.Type) << 32u) | resource.PhysicalIndex;
                const auto existing = m_FramePhysicalIds.find(frameKey);
                if (existing != m_FramePhysicalIds.end())
                {
                    binding.PhysicalId = existing->second;
                    continue;
                }

                const size_t slotIndex = resource.Desc.Type == RenderGraphResourceType::Texture
                                           ? resource.PhysicalIndex
                                           : static_cast<size_t>(graph.PhysicalTextureCount) + resource.PhysicalIndex;
                CW_ENGINE_ASSERT(slotIndex < transientSlots.size(), "Invalid transient physical index");
                TransientSlot& slot = transientSlots[slotIndex];
                if (!slot.Initialized || !DescriptorsMatch(slot.Desc, resource.Desc))
                {
                    ReleasePhysicalResource(slot.PhysicalId);
                    slot.Desc = resource.Desc;
                    slot.PhysicalId = AllocatePhysicalId();
                    slot.Initialized = true;
                    m_Stats.TransientAllocations++;
                }
                binding.PhysicalId = slot.PhysicalId;
                RegisterPhysicalResource(binding.PhysicalId, resource);
                m_FramePhysicalIds.emplace(frameKey, binding.PhysicalId);
                continue;
            }

            HistoryEntry& history = m_History[historyNamespace][resource.Desc.HistoryId];
            const bool paired = resource.Desc.HistoryRole != RenderGraphHistoryRole::Single;
            if (history.PhysicalIds[0] == 0 || history.Paired != paired || !DescriptorsMatch(history.Desc, resource.Desc))
            {
                ReleasePhysicalResource(history.PhysicalIds[0]);
                if (history.PhysicalIds[1] != history.PhysicalIds[0])
                    ReleasePhysicalResource(history.PhysicalIds[1]);
                history = {};
                history.Desc = resource.Desc;
                history.Paired = paired;
                history.PhysicalIds[0] = AllocatePhysicalId();
                history.PhysicalIds[1] = paired ? AllocatePhysicalId() : history.PhysicalIds[0];
                m_Stats.HistoryAllocations += paired ? 2u : 1u;
            }

            uint32_t physicalSlot = 0;
            if (paired)
            {
                const uint32_t writeSlot = static_cast<uint32_t>(frameNumber & 1ull);
                physicalSlot = resource.Desc.HistoryRole == RenderGraphHistoryRole::Write ? writeSlot : 1u - writeSlot;
            }
            binding.PhysicalId = history.PhysicalIds[physicalSlot];
            binding.HistoryValid = history.Valid && resource.Desc.HistoryRole != RenderGraphHistoryRole::Write;
            RegisterPhysicalResource(binding.PhysicalId, resource);
        }
        return true;
    }

    void RenderGraphResourceRegistry::EndFrame()
    {
        if (m_CurrentGraph == nullptr)
            return;
        for (const RenderGraphResourceInfo& resource : m_CurrentGraph->Resources)
        {
            if (resource.Desc.Lifetime != RenderGraphResourceLifetime::History ||
                resource.Desc.HistoryRole == RenderGraphHistoryRole::Read ||
                resource.FirstUse == RenderGraphPassHandle::InvalidIndex)
                continue;
            m_History[m_CurrentHistoryNamespace][resource.Desc.HistoryId].Valid = true;
        }
        m_CurrentGraph = nullptr;
        m_CurrentFrame = 0;
        m_CurrentHistoryNamespace = 0;
    }

    void RenderGraphResourceRegistry::InvalidateHistory(uint64_t historyNamespace)
    {
        const auto namespaceEntry = m_History.find(historyNamespace);
        if (namespaceEntry == m_History.end())
            return;
        for (auto& [_, history] : namespaceEntry->second)
        {
            if (history.Valid)
            {
                history.Valid = false;
                m_Stats.HistoryInvalidations++;
            }
        }
    }

    bool RenderGraphResourceRegistry::ReleaseHistory(uint64_t historyNamespace)
    {
        if (m_CurrentGraph != nullptr && m_CurrentHistoryNamespace == historyNamespace)
            return false;
        const auto namespaceEntry = m_History.find(historyNamespace);
        if (namespaceEntry == m_History.end())
            return false;

        for (const auto& [_, history] : namespaceEntry->second)
        {
            ReleasePhysicalResource(history.PhysicalIds[0]);
            if (history.PhysicalIds[1] != history.PhysicalIds[0])
                ReleasePhysicalResource(history.PhysicalIds[1]);
        }
        m_History.erase(namespaceEntry);
        return true;
    }

    void RenderGraphResourceRegistry::Reset()
    {
        m_TransientFrames.assign(m_FramesInFlight, {});
        m_History.clear();
        m_PhysicalResources.clear();
        m_RenderTargets.clear();
        m_FramePhysicalIds = {};
        m_Bindings.clear();
        m_CurrentGraph = nullptr;
        m_CurrentFrame = 0;
        m_CurrentHistoryNamespace = 0;
        m_NextPhysicalId = 1;
        m_FramePhysicalScratchCapacity = 0;
        m_FrameAllocationFailed = false;
        m_AllocationError.clear();
        m_Stats = {};
    }

    const RenderGraphResourceBinding& RenderGraphResourceRegistry::Get(RenderGraphResourceHandle handle)
    {
        CW_ENGINE_ASSERT(m_CurrentGraph != nullptr, "Render graph resources are only valid during a frame");
        CW_ENGINE_ASSERT(handle.IsValid() && handle.Index < m_Bindings.size(), "Invalid render graph resource handle");
        const RenderGraphResourceInfo& resource = m_CurrentGraph->Resources[handle.Index];
        CW_ENGINE_ASSERT(resource.Handle.Generation == handle.Generation && resource.Handle.Type == handle.Type,
                         "Stale render graph resource handle");
        RenderGraphResourceBinding& binding = m_Bindings[handle.Index];
        ResolvePhysicalResource(binding);
        return binding;
    }

    const Ref<Texture>& RenderGraphResourceRegistry::GetTexture(RenderGraphResourceHandle handle)
    {
        const RenderGraphResourceBinding& binding = Get(handle);
        CW_ENGINE_ASSERT(binding.Type == RenderGraphResourceType::Texture, "Render graph resource is not a texture");
        return binding.TextureResource;
    }

    const Ref<GenericGpuBuffer>& RenderGraphResourceRegistry::GetBuffer(RenderGraphResourceHandle handle)
    {
        const RenderGraphResourceBinding& binding = Get(handle);
        CW_ENGINE_ASSERT(binding.Type == RenderGraphResourceType::Buffer, "Render graph resource is not a buffer");
        return binding.BufferResource;
    }

    const Ref<RenderTarget>& RenderGraphResourceRegistry::GetRenderTarget(RenderGraphResourceHandle handle)
    {
        const RenderGraphResourceBinding& binding = Get(handle);
        CW_ENGINE_ASSERT(binding.Type == RenderGraphResourceType::Texture, "Render graph resource is not a texture");
        return binding.RenderTargetResource;
    }

    Ref<RenderTarget> RenderGraphResourceRegistry::GetRenderTarget(const RenderGraphRenderTargetDesc& desc)
    {
        if (m_CurrentGraph == nullptr || desc.ColorCount > MAX_FRAMEBUFFER_COLOR_ATTACHMENTS ||
            (desc.ColorCount == 0 && !desc.Depth.IsValid()))
            return nullptr;

        RenderTargetKey key;
        key.ColorCount = desc.ColorCount;
        key.MipLevel = desc.MipLevel;
        key.FirstLayer = desc.FirstLayer;
        key.LayerCount = std::max(desc.LayerCount, 1u);
        RenderTextureDesc targetDesc;
        bool dimensionsSet = false;
        auto bindAttachment = [&](RenderGraphResourceHandle handle, RenderTextureSurface& surface,
                                  uint32_t keyIndex) -> bool {
            if (!handle.IsValid() || handle.Type != RenderGraphResourceType::Texture ||
                handle.Index >= m_CurrentGraph->Resources.size())
                return false;
            const RenderGraphResourceInfo& resource = m_CurrentGraph->Resources[handle.Index];
            const RenderGraphResourceBinding& binding = Get(handle);
            if (!binding.TextureResource)
                return false;
            const uint32_t width = std::max(resource.Desc.Texture.Width >> desc.MipLevel, 1u);
            const uint32_t height = std::max(resource.Desc.Texture.Height >> desc.MipLevel, 1u);
            const uint32_t samples = std::max(resource.Desc.Texture.Samples, 1u);
            if (desc.MipLevel >= std::max(resource.Desc.Texture.MipLevels, 1u) ||
                desc.FirstLayer + key.LayerCount > std::max(resource.Desc.Texture.Layers, 1u))
                return false;
            if (dimensionsSet && (key.Width != width || key.Height != height || key.Samples != samples))
                return false;
            key.Width = width;
            key.Height = height;
            key.Samples = samples;
            key.PhysicalIds[keyIndex] = binding.PhysicalId;
            dimensionsSet = true;
            surface.Texture = binding.TextureResource;
            surface.Face = desc.FirstLayer;
            surface.NumFaces = key.LayerCount;
            surface.MipLevel = desc.MipLevel;
            return true;
        };

        for (uint32_t index = 0; index < desc.ColorCount; index++)
        {
            if (!bindAttachment(desc.Colors[index], targetDesc.ColorSurfaces[index], index))
                return nullptr;
        }
        if (desc.Depth.IsValid() &&
            !bindAttachment(desc.Depth, targetDesc.DepthSurface, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS))
            return nullptr;

        const auto cached = m_RenderTargets.find(key);
        if (cached != m_RenderTargets.end())
        {
            m_Stats.RenderTargetCacheHits++;
            return cached->second;
        }

        targetDesc.Width = key.Width;
        targetDesc.Height = key.Height;
        targetDesc.Samples = key.Samples;
        targetDesc.NumSlices = key.LayerCount;
        Ref<RenderTarget> target = RenderTexture::Create(targetDesc);
        if (!target)
            return nullptr;
        m_RenderTargets.emplace(key, target);
        m_Stats.RenderTargetAllocations++;
        return target;
    }

    bool RenderGraphResourceRegistry::BindExternalTexture(RenderGraphResourceHandle handle, const Ref<Texture>& texture)
    {
        if (m_CurrentGraph == nullptr || !handle.IsValid() || handle.Index >= m_Bindings.size() ||
            handle.Type != RenderGraphResourceType::Texture)
            return false;
        RenderGraphResourceBinding& binding = m_Bindings[handle.Index];
        if (binding.ExternalId == 0 || reinterpret_cast<uint64_t>(texture.get()) != binding.ExternalId)
            return false;
        binding.TextureResource = texture;
        binding.RenderTargetResource = nullptr;
        binding.AllocationAttempted = true;
        return true;
    }

    bool RenderGraphResourceRegistry::BindExternalRenderTarget(RenderGraphResourceHandle handle,
                                                                const Ref<RenderTarget>& renderTarget)
    {
        if (m_CurrentGraph == nullptr || !handle.IsValid() || handle.Index >= m_Bindings.size() ||
            handle.Type != RenderGraphResourceType::Texture)
            return false;
        RenderGraphResourceBinding& binding = m_Bindings[handle.Index];
        if (binding.ExternalId == 0 || reinterpret_cast<uint64_t>(renderTarget.get()) != binding.ExternalId)
            return false;
        binding.TextureResource = nullptr;
        binding.RenderTargetResource = renderTarget;
        binding.AllocationAttempted = true;
        return true;
    }

    bool RenderGraphResourceRegistry::BindExternalBuffer(RenderGraphResourceHandle handle,
                                                         const Ref<GenericGpuBuffer>& buffer)
    {
        if (m_CurrentGraph == nullptr || !handle.IsValid() || handle.Index >= m_Bindings.size() ||
            handle.Type != RenderGraphResourceType::Buffer)
            return false;
        RenderGraphResourceBinding& binding = m_Bindings[handle.Index];
        if (binding.ExternalId == 0 || reinterpret_cast<uint64_t>(buffer.get()) != binding.ExternalId)
            return false;
        binding.BufferResource = buffer;
        binding.AllocationAttempted = true;
        return true;
    }

    bool RenderGraphResourceRegistry::DescriptorsMatch(const RenderGraphResourceDesc& first,
                                                       const RenderGraphResourceDesc& second)
    {
        if (first.Type != second.Type)
            return false;
        return first.Type == RenderGraphResourceType::Texture ? first.Texture == second.Texture
                                                               : first.Buffer == second.Buffer;
    }

    size_t RenderGraphResourceRegistry::RenderTargetKeyHash::operator()(const RenderTargetKey& key) const
    {
        size_t hash = 1469598103934665603ull;
        const auto combine = [&](uint64_t value) {
            hash ^= static_cast<size_t>(value);
            hash *= 1099511628211ull;
        };
        for (uint64_t physicalId : key.PhysicalIds)
            combine(physicalId);
        combine(key.ColorCount);
        combine(key.MipLevel);
        combine(key.FirstLayer);
        combine(key.LayerCount);
        combine(key.Width);
        combine(key.Height);
        combine(key.Samples);
        return hash;
    }

    uint64_t RenderGraphResourceRegistry::AllocatePhysicalId()
    {
        const uint64_t result = m_NextPhysicalId++;
        if (m_NextPhysicalId == 0)
            m_NextPhysicalId = 1;
        return result;
    }

    void RenderGraphResourceRegistry::RegisterPhysicalResource(uint64_t physicalId,
                                                               const RenderGraphResourceInfo& resource)
    {
        if (physicalId == 0)
            return;
        const auto [entry, inserted] = m_PhysicalResources.try_emplace(physicalId);
        if (inserted)
        {
            entry->second.Desc = resource.Desc;
            entry->second.Name = resource.Name;
        }
        else
        {
            CW_ENGINE_ASSERT(DescriptorsMatch(entry->second.Desc, resource.Desc),
                             "Aliased render graph resources have incompatible descriptors");
        }
    }

    void RenderGraphResourceRegistry::ReleasePhysicalResource(uint64_t physicalId)
    {
        if (physicalId != 0)
        {
            for (auto target = m_RenderTargets.begin(); target != m_RenderTargets.end();)
            {
                const bool referencesResource =
                  std::find(target->first.PhysicalIds.begin(), target->first.PhysicalIds.end(), physicalId) !=
                  target->first.PhysicalIds.end();
                target = referencesResource ? m_RenderTargets.erase(target) : std::next(target);
            }
            const auto physical = m_PhysicalResources.find(physicalId);
            if (physical != m_PhysicalResources.end() && m_Allocator != nullptr)
            {
                if (physical->second.TextureResource)
                    m_Allocator->ReleaseTexture(physical->second.Desc.Texture, std::move(physical->second.TextureResource));
                if (physical->second.BufferResource)
                    m_Allocator->ReleaseBuffer(physical->second.Desc.Buffer, std::move(physical->second.BufferResource));
            }
            m_PhysicalResources.erase(physicalId);
        }
    }

    void RenderGraphResourceRegistry::ResolvePhysicalResource(RenderGraphResourceBinding& binding)
    {
        if (binding.ExternalId != 0 || binding.PhysicalId == 0)
            return;

        const auto physicalEntry = m_PhysicalResources.find(binding.PhysicalId);
        CW_ENGINE_ASSERT(physicalEntry != m_PhysicalResources.end(), "Render graph physical resource is not registered");
        PhysicalResource& physical = physicalEntry->second;
        if (!physical.AllocationAttempted && m_Allocator != nullptr)
        {
            physical.AllocationAttempted = true;
            try
            {
                if (physical.Desc.Type == RenderGraphResourceType::Texture)
                {
                    physical.TextureResource = m_Allocator->CreateTexture(physical.Name, physical.Desc.Texture);
                    if (physical.TextureResource)
                        m_Stats.TextureAllocations++;
                }
                else
                {
                    physical.BufferResource = m_Allocator->CreateBuffer(physical.Name, physical.Desc.Buffer);
                    if (physical.BufferResource)
                        m_Stats.BufferAllocations++;
                }
            }
            catch (const std::exception& exception)
            {
                CW_ENGINE_ERROR("Failed to allocate render graph resource '{}': {}", physical.Name, exception.what());
            }

            if (!physical.TextureResource && !physical.BufferResource)
                m_Stats.AllocationFailures++;
        }

        if (physical.AllocationAttempted && !physical.TextureResource && !physical.BufferResource)
        {
            m_FrameAllocationFailed = true;
            m_AllocationError = "Failed to allocate render graph resource '" + physical.Name + "'";
        }

        binding.TextureResource = physical.TextureResource;
        binding.BufferResource = physical.BufferResource;
        binding.AllocationAttempted = physical.AllocationAttempted;
    }
} // namespace Crowny
