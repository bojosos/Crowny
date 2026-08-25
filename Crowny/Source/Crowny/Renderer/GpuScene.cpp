#include "cwpch.h"

#include "Crowny/Renderer/GpuScene.h"

#include "Crowny/RenderAPI/RenderCapabilities.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/Renderer/Renderer.h"

#include <bit>

namespace Crowny
{
    namespace
    {
        constexpr uint32_t MinimumGeometryVertexCapacity = 8u * 1024u * 1024u;
        constexpr uint32_t MaximumGeometryVertexCapacity = 64u * 1024u * 1024u;
        constexpr uint32_t MinimumGeometryIndexCapacity = 2u * 1024u * 1024u;
        constexpr uint32_t MaximumGeometryIndexCapacity = 16u * 1024u * 1024u;

        uint32_t NextCapacity(uint32_t required, uint32_t minimum) { return std::max(std::bit_ceil(std::max(required, 1u)), minimum); }

        uint32_t GeometryPageCapacity(uint32_t required, uint32_t minimum, uint32_t maximum)
        {
            if (required == 0 || required > maximum)
                return 0;
            const uint64_t preferred = std::max<uint64_t>(minimum, static_cast<uint64_t>(required) * 4u);
            const uint32_t clamped = static_cast<uint32_t>(std::min<uint64_t>(preferred, maximum));
            return std::bit_ceil(clamped);
        }

        template <typename T> bool ReadMaterialValue(const Material& material, std::initializer_list<StringView> names, T& output)
        {
            for (StringView name : names)
            {
                const auto binding = material.GetBindings().find(name);
                if (binding == material.GetBindings().end() || binding->second.DataType != ShaderDataTypeTrait<T>::Type)
                    continue;
                output = material.GetDataParam<T>(String(name));
                return true;
            }
            return false;
        }

        Ref<Texture> FindMaterialTexture(const Material& material, std::initializer_list<StringView> names)
        {
            for (StringView name : names)
            {
                const AssetHandle<Texture> handle = material.GetTextureHandle(String(name));
                if (handle)
                    return handle.GetInternalPtr();
            }
            if (material.GetPassCount() == 0)
                return nullptr;
            const UniformDesc::TextureMap textures = material.GetTextures();
            for (StringView name : names)
            {
                const auto texture = textures.find(name);
                if (texture != textures.end())
                    return material.GetTexture(texture->second.Set, texture->second.Slot);
            }
            return nullptr;
        }

        AlphaMode GetMaterialAlpha(const Vector<GpuMaterialData>& materials, uint32_t materialIndex)
        {
            if (materialIndex >= materials.size())
                return AlphaMode::Opaque;
            const uint32_t packedAlpha = (materials[materialIndex].TextureIndices1.w >> 8u) & 0xffu;
            return packedAlpha <= static_cast<uint32_t>(AlphaMode::WeightedOIT) ? static_cast<AlphaMode>(packedAlpha) : AlphaMode::Opaque;
        }

        GpuDrawBinKey StandardDrawBin(AlphaMode alpha, uint32_t geometryHeap)
        {
            GpuDrawBinKey key;
            key.Phase = alpha == AlphaMode::Opaque || alpha == AlphaMode::Mask ? RenderDrawPhase::Opaque : RenderDrawPhase::Transparent;
            key.Alpha = alpha;
            key.GeometryHeap = geometryHeap;
            return key;
        }
    } // namespace

    GpuScene::GpuScene(bool enableGpuBuffers) : m_EnableGpuBuffers(enableGpuBuffers), m_DrawBuffers(enableGpuBuffers)
    {
        m_DirtyInstanceIndices.reserve(1024);
        m_DirtyLightIndices.reserve(256);
        m_DirtyMeshIndices.reserve(64);
        m_InstanceRanges.reserve(64);
        m_LightRanges.reserve(32);
        m_MeshRanges.reserve(32);
        m_MeshLodRanges.reserve(32);
        m_MeshletRanges.reserve(64);
    }

    void GpuScene::BeginFrame(uint64_t frameNumber)
    {
        m_LodTableAllocator.BeginFrame(frameNumber);
        m_MeshletTableAllocator.BeginFrame(frameNumber);
        for (GeometryHeapPage& page : m_GeometryHeapPages)
            page.Heap->BeginFrame(frameNumber);
        UpdateGeometryHeapStats();
    }

    void GpuScene::Apply(const RenderWorldChange* instanceChanges, uint32_t instanceChangeCount, const RenderLightChange* lightChanges,
                         uint32_t lightChangeCount)
    {
        m_Stats.UploadedBytes = 0;
        m_Stats.InstanceRanges = 0;
        m_Stats.LightRanges = 0;
        m_Stats.ShadowRanges = 0;
        m_Stats.MeshRanges = 0;
        m_Stats.MaterialRanges = 0;
        m_Stats.GeometryUploadBytes = 0;
        m_DirtyInstanceIndices.clear();
        m_DirtyLightIndices.clear();

        uint32_t requiredInstances = static_cast<uint32_t>(m_Instances.size());
        for (uint32_t index = 0; instanceChanges != nullptr && index < instanceChangeCount; index++)
            if (instanceChanges[index].Handle.IsValid())
                requiredInstances = std::max(requiredInstances, instanceChanges[index].Handle.GetIndex() + 1u);
        const bool instanceBufferRecreated = EnsureInstanceCapacity(requiredInstances);

        for (uint32_t index = 0; instanceChanges != nullptr && index < instanceChangeCount; index++)
        {
            const RenderWorldChange& change = instanceChanges[index];
            if (!change.Handle.IsValid() || change.Type == RenderWorldChangeType::Cancelled)
                continue;
            const uint32_t slotIndex = change.Handle.GetIndex();
            SlotState& state = m_InstanceStates[slotIndex];
            if (change.Type == RenderWorldChangeType::Destroy)
            {
                if (state.Alive && state.Generation == change.Handle.GetGeneration())
                {
                    m_DrawBinsDirty = true;
                    state.Alive = false;
                    m_Instances[slotIndex] = {};
                    m_Stats.ActiveInstances--;
                    m_DirtyInstanceIndices.push_back(slotIndex);
                }
                continue;
            }
            if (change.Type == RenderWorldChangeType::Update && (!state.Alive || state.Generation != change.Handle.GetGeneration()))
                continue;
            if (!state.Alive)
                m_Stats.ActiveInstances++;
            if (change.Type != RenderWorldChangeType::Update ||
                (static_cast<uint8_t>(change.DirtyFlags) & static_cast<uint8_t>(RenderWorldDirtyFlags::Draw)) != 0)
                m_DrawBinsDirty = true;
            state.Alive = true;
            state.Generation = change.Handle.GetGeneration();
            m_Instances[slotIndex] = change.Data;
            m_DirtyInstanceIndices.push_back(slotIndex);
        }

        uint32_t requiredLights = static_cast<uint32_t>(m_Lights.size());
        for (uint32_t index = 0; lightChanges != nullptr && index < lightChangeCount; index++)
            if (lightChanges[index].Handle.IsValid())
                requiredLights = std::max(requiredLights, lightChanges[index].Handle.GetIndex() + 1u);
        const bool lightBufferRecreated = EnsureLightCapacity(requiredLights);

        for (uint32_t index = 0; lightChanges != nullptr && index < lightChangeCount; index++)
        {
            const RenderLightChange& change = lightChanges[index];
            if (!change.Handle.IsValid() || change.Type == RenderLightChangeType::Cancelled)
                continue;
            const uint32_t slotIndex = change.Handle.GetIndex();
            SlotState& state = m_LightStates[slotIndex];
            if (change.Type == RenderLightChangeType::Destroy)
            {
                if (state.Alive && state.Generation == change.Handle.GetGeneration())
                {
                    state.Alive = false;
                    m_Lights[slotIndex] = {};
                    m_Stats.ActiveLights--;
                    m_DirtyLightIndices.push_back(slotIndex);
                }
                continue;
            }
            if (change.Type == RenderLightChangeType::Update && (!state.Alive || state.Generation != change.Handle.GetGeneration()))
                continue;
            if (!state.Alive)
                m_Stats.ActiveLights++;
            state.Alive = true;
            state.Generation = change.Handle.GetGeneration();
            m_Lights[slotIndex] = change.Data;
            m_DirtyLightIndices.push_back(slotIndex);
        }

        if (instanceBufferRecreated)
        {
            m_InstanceBuffer->WriteData(0, static_cast<uint32_t>(m_Instances.size() * sizeof(RenderInstanceData)), m_Instances.data(), BWT_DISCARD);
            m_Stats.UploadedBytes += m_Instances.size() * sizeof(RenderInstanceData);
            m_Stats.InstanceRanges = 1;
        }
        else
        {
            BuildDirtyRanges(m_DirtyInstanceIndices, m_InstanceRanges);
            FlushInstanceRanges(m_InstanceRanges);
        }
        if (lightBufferRecreated)
        {
            m_LightBuffer->WriteData(0, static_cast<uint32_t>(m_Lights.size() * sizeof(RenderLightData)), m_Lights.data(), BWT_DISCARD);
            m_Stats.UploadedBytes += m_Lights.size() * sizeof(RenderLightData);
            m_Stats.LightRanges = 1;
        }
        else
        {
            BuildDirtyRanges(m_DirtyLightIndices, m_LightRanges);
            FlushLightRanges(m_LightRanges);
        }
        m_Stats.InstanceCapacity = static_cast<uint32_t>(m_Instances.size());
        m_Stats.LightCapacity = static_cast<uint32_t>(m_Lights.size());
    }

    void GpuScene::ApplyResources(const RenderMeshResourceChange* meshChanges, uint32_t meshChangeCount,
                                  const RenderMaterialResourceChange* materialChanges, uint32_t materialChangeCount)
    {
        bool materialsDirty = false;
        bool meshesDirty = false;
        m_DirtyMeshIndices.clear();
        m_MeshRanges.clear();
        m_MeshLodRanges.clear();
        m_MeshletRanges.clear();
        for (uint32_t changeIndex = 0; meshChanges != nullptr && changeIndex < meshChangeCount; changeIndex++)
        {
            const RenderMeshResourceChange& change = meshChanges[changeIndex];
            if (change.Index == 0)
                continue;
            if (change.Index >= m_MeshResources.size())
                m_MeshResources.resize(change.Index + 1u);
            MeshResourceState& state = m_MeshResources[change.Index];
            if (change.Type == RenderResourceChangeType::Destroy)
            {
                if (!state.Resource && state.Version == 0)
                    continue;
                ReleaseGeometryResource(change.Index);
                state.Resource = nullptr;
                state.Version = 0;
            }
            else
            {
                if (state.Resource.GetHandleData().get() == change.Resource.GetHandleData().get() && state.Version == change.Version)
                    continue;
                ReleaseGeometryResource(change.Index);
                state.Resource = change.Resource;
                state.Version = change.Version;
                UpdateGeometryResource(change.Index);
            }
            meshesDirty = true;
        }

        for (uint32_t changeIndex = 0; materialChanges != nullptr && changeIndex < materialChangeCount; changeIndex++)
        {
            const RenderMaterialResourceChange& change = materialChanges[changeIndex];
            if (change.Index == 0)
                continue;
            if (change.Index >= m_MaterialResources.size())
                m_MaterialResources.resize(change.Index + 1u);
            MaterialResourceState& state = m_MaterialResources[change.Index];
            if (change.Type == RenderResourceChangeType::Destroy)
            {
                if (!state.Resource && state.Version == 0)
                    continue;
                state = {};
            }
            else
            {
                if (state.Resource.GetHandleData().get() == change.Resource.GetHandleData().get() && state.Version == change.Version)
                    continue;
                state.Resource = change.Resource;
                state.Version = change.Version;
            }
            materialsDirty = true;
        }

        FlushGeometryTables();
        UpdateGeometryHeapStats();
        if (materialsDirty)
            RebuildMaterialTable();
        if (meshesDirty || materialsDirty)
            m_DrawBinsDirty = true;
    }

    void GpuScene::Reset()
    {
        m_Instances.clear();
        m_Lights.clear();
        m_ShadowLights.clear();
        m_ShadowViews.clear();
        m_MeshResources.clear();
        m_MaterialResources.clear();
        m_GeometryHeapPages.clear();
        m_GeometryResidencyFailures = 0;
        m_Geometry = {};
        m_LodTableAllocator.Reset();
        m_MeshletTableAllocator.Reset();
        m_Materials.clear();
        m_MeshIndexBuffers.clear();
        m_BindlessTextureResources.clear();
        m_BindlessTextures.reset();
        m_DrawCandidates.clear();
        m_DrawBuffers.Reset();
        m_DrawBinLayout.Reset();
        m_DrawBinKeys.clear();
        m_DrawBinBuffer = nullptr;
        m_DrawBinsDirty = true;
        m_InstanceStates.clear();
        m_LightStates.clear();
        m_InstanceBuffer = nullptr;
        m_LightBuffer = nullptr;
        m_ShadowLightBuffer = nullptr;
        m_ShadowViewBuffer = nullptr;
        m_MeshBuffer = nullptr;
        m_MeshLodBuffer = nullptr;
        m_MeshletBuffer = nullptr;
        m_MaterialBuffer = nullptr;
        m_Stats = {};
    }

    void GpuScene::UploadShadowData(const GpuShadowLightData* lights, uint32_t lightCount, const GpuShadowViewData* views, uint32_t viewCount)
    {
        lightCount = std::min(lightCount, RenderLightHandle::MaxLights);
        viewCount = std::min(viewCount, 4096u);
        const bool lightsChanged = m_ShadowLights.size() != lightCount ||
                                   (lightCount != 0 && std::memcmp(m_ShadowLights.data(), lights, lightCount * sizeof(GpuShadowLightData)) != 0);
        const bool viewsChanged = m_ShadowViews.size() != viewCount ||
                                  (viewCount != 0 && std::memcmp(m_ShadowViews.data(), views, viewCount * sizeof(GpuShadowViewData)) != 0);
        if (!lightsChanged && !viewsChanged)
            return;

        if (lightsChanged)
        {
            if (lightCount == 0)
                m_ShadowLights.clear();
            else
                m_ShadowLights.assign(lights, lights + lightCount);
        }
        if (viewsChanged)
        {
            if (viewCount == 0)
                m_ShadowViews.clear();
            else
                m_ShadowViews.assign(views, views + viewCount);
        }

        const bool lightBufferRecreated = EnsureShadowLightCapacity(lightCount);
        const bool viewBufferRecreated = EnsureShadowViewCapacity(viewCount);
        if (lightsChanged && m_ShadowLightBuffer && !m_ShadowLights.empty())
        {
            const uint32_t size = static_cast<uint32_t>(m_ShadowLights.size() * sizeof(GpuShadowLightData));
            m_ShadowLightBuffer->WriteData(0, size, m_ShadowLights.data(), lightBufferRecreated ? BWT_DISCARD : BWT_NORMAL);
            m_Stats.UploadedBytes += size;
            m_Stats.ShadowRanges++;
        }
        if (viewsChanged && m_ShadowViewBuffer && !m_ShadowViews.empty())
        {
            const uint32_t size = static_cast<uint32_t>(m_ShadowViews.size() * sizeof(GpuShadowViewData));
            m_ShadowViewBuffer->WriteData(0, size, m_ShadowViews.data(), viewBufferRecreated ? BWT_DISCARD : BWT_NORMAL);
            m_Stats.UploadedBytes += size;
            m_Stats.ShadowRanges++;
        }
        m_Stats.ShadowLightCapacity = m_ShadowLightBuffer ? m_ShadowLightBuffer->GetSize() / sizeof(GpuShadowLightData) : 0;
        m_Stats.ShadowViewCapacity = m_ShadowViewBuffer ? m_ShadowViewBuffer->GetSize() / sizeof(GpuShadowViewData) : 0;
    }

    bool GpuScene::TryGetInstance(RenderInstanceHandle handle, RenderInstanceData& output) const
    {
        if (!handle.IsValid() || handle.GetIndex() >= m_InstanceStates.size())
            return false;
        const SlotState& state = m_InstanceStates[handle.GetIndex()];
        if (!state.Alive || state.Generation != handle.GetGeneration())
            return false;
        output = m_Instances[handle.GetIndex()];
        return true;
    }

    bool GpuScene::TryGetLight(RenderLightHandle handle, RenderLightData& output) const
    {
        if (!handle.IsValid() || handle.GetIndex() >= m_LightStates.size())
            return false;
        const SlotState& state = m_LightStates[handle.GetIndex()];
        if (!state.Alive || state.Generation != handle.GetGeneration())
            return false;
        output = m_Lights[handle.GetIndex()];
        return true;
    }

    Ref<VertexBuffer> GpuScene::GetGeometryVertexBuffer(uint32_t geometryBinding) const
    {
        if (!IsPerMeshGeometryBinding(geometryBinding))
        {
            const GeometryHeapPage* page = GetGeometryHeapPage(geometryBinding);
            return page != nullptr && page->Heap ? page->Heap->GetVertexBuffer() : nullptr;
        }
        const uint32_t meshIndex = GetPerMeshGeometryIndex(geometryBinding);
        if (meshIndex >= m_MeshResources.size() || !m_MeshResources[meshIndex].Resource)
            return nullptr;
        return m_MeshResources[meshIndex].Resource->GetVertexBuffer();
    }

    Ref<IndexBuffer> GpuScene::GetGeometryIndexBuffer(uint32_t geometryBinding) const
    {
        if (!IsPerMeshGeometryBinding(geometryBinding))
        {
            const GeometryHeapPage* page = GetGeometryHeapPage(geometryBinding);
            return page != nullptr && page->Heap ? page->Heap->GetIndexBuffer() : nullptr;
        }
        const uint32_t meshIndex = GetPerMeshGeometryIndex(geometryBinding);
        if (meshIndex >= m_MeshIndexBuffers.size())
            return nullptr;
        return m_MeshIndexBuffers[meshIndex];
    }

    DrawMode GpuScene::GetGeometryDrawMode(uint32_t geometryBinding) const
    {
        if (!IsPerMeshGeometryBinding(geometryBinding))
        {
            const GeometryHeapPage* page = GetGeometryHeapPage(geometryBinding);
            return page != nullptr ? page->Topology : DrawMode::TRIANGLE_LIST;
        }
        const uint32_t meshIndex = GetPerMeshGeometryIndex(geometryBinding);
        return meshIndex < m_MeshResources.size() && m_MeshResources[meshIndex].Resource ? m_MeshResources[meshIndex].Resource->GetDrawMode()
                                                                                         : DrawMode::TRIANGLE_LIST;
    }

    const AssetHandle<Mesh>& GpuScene::GetMeshResource(uint32_t meshIndex) const
    {
        static const AssetHandle<Mesh> empty;
        return meshIndex < m_MeshResources.size() ? m_MeshResources[meshIndex].Resource : empty;
    }

    const AssetHandle<Material>& GpuScene::GetMaterialResource(uint32_t materialIndex) const
    {
        static const AssetHandle<Material> empty;
        return materialIndex < m_MaterialResources.size() ? m_MaterialResources[materialIndex].Resource : empty;
    }

    void GpuScene::DrainBindlessTextureUpdates(Vector<BindlessResourceUpdate>& output)
    {
        if (m_BindlessTextures)
            m_BindlessTextures->DrainUpdates(output);
        else
            output.clear();
    }

    void GpuScene::PrepareGpuDrawBins(const GpuDrawBinLayoutDesc& desc)
    {
        bool layoutChanged = false;
        if (m_DrawBinsDirty || !m_DrawBinLayout.Matches(desc))
        {
            CollectGpuDrawBinKeys(m_DrawBinKeys);
            layoutChanged = m_DrawBinLayout.Build(m_DrawBinKeys.data(), static_cast<uint32_t>(m_DrawBinKeys.size()), desc);
            m_DrawBinsDirty = false;
        }

        const Vector<GpuDrawBinLookupEntry>& lookupEntries = m_DrawBinLayout.GetLookupEntries();
        if (lookupEntries.empty() || !CanCreateGpuBuffers())
            m_DrawBinBuffer = nullptr;
        else if (layoutChanged || !m_DrawBinBuffer)
        {
            const uint32_t requiredSize = static_cast<uint32_t>(lookupEntries.size() * sizeof(GpuDrawBinLookupEntry));
            if (!m_DrawBinBuffer || m_DrawBinBuffer->GetSize() != requiredSize)
            {
                GenericGpuBufferDesc bufferDesc;
                bufferDesc.ElementCount = static_cast<uint32_t>(lookupEntries.size());
                bufferDesc.ElementSize = sizeof(GpuDrawBinLookupEntry);
                bufferDesc.Type = GpuBufferType::Structured;
                bufferDesc.Usage = BufferUsage::BU_LOADSTORE;
                m_DrawBinBuffer = GenericGpuBuffer::Create(bufferDesc);
            }
            if (m_DrawBinBuffer)
            {
                m_DrawBinBuffer->WriteData(0, requiredSize, lookupEntries.data(), BWT_DISCARD);
                m_Stats.UploadedBytes += requiredSize;
            }
        }

        const GpuDrawBinLayoutStats& layoutStats = m_DrawBinLayout.GetStats();
        m_Stats.DrawBinCount = layoutStats.ActiveBinCount;
        m_Stats.RejectedDrawBins = layoutStats.RejectedBinCount;
        m_Stats.DrawBinCommandCapacity = layoutStats.CommandCapacity;
        m_Stats.DrawBinLookupCapacity = layoutStats.LookupCapacity;
    }

    void GpuScene::CollectGpuDrawBinKeys(Vector<GpuDrawBinKey>& output) const
    {
        output.clear();
        for (uint32_t instanceIndex = 0; instanceIndex < m_InstanceStates.size(); instanceIndex++)
        {
            if (!m_InstanceStates[instanceIndex].Alive)
                continue;
            const RenderInstanceData& instance = m_Instances[instanceIndex];
            if (!HasFlag(RenderWorld::GetFlags(instance.Draw), RenderInstanceFlags::Visible))
                continue;
            const uint32_t meshIndex = RenderWorld::GetMeshHandle(instance.Draw);
            if (meshIndex == 0 || meshIndex >= m_MeshResources.size())
                continue;
            const MeshResourceState& mesh = m_MeshResources[meshIndex];
            if (!mesh.Resource || !mesh.Meshlets || IsPerMeshGeometryBinding(mesh.GeometryBinding))
                continue;
            for (uint32_t meshletOffset = 0; meshletOffset < mesh.Meshlets.Count; meshletOffset++)
            {
                const GpuMeshletData& meshlet = m_Geometry.Meshlets[mesh.Meshlets.First + meshletOffset];
                if (meshlet.Draw.y == 0)
                    continue;
                const uint32_t materialIndex = RenderWorld::GetMaterialHandle(instance.Draw) + meshlet.Draw.z;
                const AlphaMode alpha = GetMaterialAlpha(m_Materials, materialIndex);
                if (alpha != AlphaMode::Opaque && alpha != AlphaMode::Mask)
                    continue;
                output.push_back(StandardDrawBin(alpha, meshlet.Geometry.z));
            }
        }
    }

    void GpuScene::BuildCpuDrawList(const RenderView& view, GpuDrawList& output, GpuDrawBuffers* outputBuffers, bool shadowCastersOnly)
    {
        const VisibilityCullingStats previousCulling = m_Stats.Culling;
        m_DrawCandidates.clear();
        m_Stats.Culling = {};
        const VisibilityFrustum frustum =
          VisibilityFrustum::FromViewProjection(view.Projection * view.View, RenderAPI::GetAPI() == RenderAPI::API::Vulkan);
        const float viewportHeight = std::max(view.ViewportSize.y, 1.0f);
        const float projectionYScale = std::abs(view.Projection[1][1]);
        for (uint32_t instanceIndex = 0; instanceIndex < m_InstanceStates.size(); instanceIndex++)
        {
            if (!m_InstanceStates[instanceIndex].Alive)
                continue;
            const RenderInstanceData& instance = m_Instances[instanceIndex];
            const RenderInstanceFlags flags = RenderWorld::GetFlags(instance.Draw);
            if (!HasFlag(flags, RenderInstanceFlags::Visible))
            {
                m_Stats.Culling.Add(VisibilityCullReason::Hidden);
                continue;
            }
            if (shadowCastersOnly && !HasFlag(flags, RenderInstanceFlags::CastShadows))
            {
                m_Stats.Culling.Add(VisibilityCullReason::Hidden);
                continue;
            }
            if ((instance.Draw.VisibilityLayerMask & view.VisibilityMask.Value) == 0)
            {
                m_Stats.Culling.Add(VisibilityCullReason::Layer);
                continue;
            }
            const glm::vec3 center(instance.Culling.BoundingSphere);
            const float radius = instance.Culling.BoundingSphere.w;
            if (!frustum.IntersectsSphere(center, radius))
            {
                m_Stats.Culling.Add(VisibilityCullReason::Frustum);
                continue;
            }

            const uint32_t meshIndex = RenderWorld::GetMeshHandle(instance.Draw);
            if (meshIndex == 0 || meshIndex >= m_Geometry.Meshes.size() || meshIndex >= m_MeshResources.size() ||
                !m_MeshResources[meshIndex].Resource)
            {
                m_Stats.Culling.Add(VisibilityCullReason::Hidden);
                continue;
            }
            const AssetHandle<Mesh>& mesh = m_MeshResources[meshIndex].Resource;
            const glm::vec3 viewCenter = glm::vec3(view.View * glm::vec4(center, 1.0f));
            const float viewDepth = std::max(-viewCenter.z, 0.0001f);
            const uint32_t lod = VisibilityCulling::SelectLod(mesh->GetGpuGeometry(), viewDepth, projectionYScale, viewportHeight, 1.0f,
                                                              RenderWorld::GetLodBias(instance.Draw));
            const GpuMeshRecord& meshRecord = m_Geometry.Meshes[meshIndex];
            if (meshRecord.LodRangeAndHeaps.y == 0)
                continue;
            const uint32_t selectedLod = std::min(lod, meshRecord.LodRangeAndHeaps.y - 1u);
            const GpuMeshLodData& lodRecord = m_Geometry.Lods[meshRecord.LodRangeAndHeaps.x + selectedLod];
            for (uint32_t meshletOffset = 0; meshletOffset < lodRecord.MeshletCount; meshletOffset++)
            {
                const GpuMeshletData& meshlet = m_Geometry.Meshlets[lodRecord.FirstMeshlet + meshletOffset];
                if (meshlet.Draw.y == 0)
                    continue;
                const uint32_t materialIndex = RenderWorld::GetMaterialHandle(instance.Draw) + meshlet.Draw.z;
                const AlphaMode alpha = GetMaterialAlpha(m_Materials, materialIndex);
                GpuDrawCandidate candidate;
                candidate.Bin = StandardDrawBin(alpha, meshlet.Geometry.z);
                candidate.InstanceID = instanceIndex;
                candidate.MaterialIndex = materialIndex;
                candidate.IndexCount = meshlet.Draw.y;
                candidate.FirstIndex = meshlet.Draw.x;
                candidate.VertexOffset = static_cast<int32_t>(meshlet.Geometry.x);
                candidate.ViewDepth = viewDepth;
                m_DrawCandidates.push_back(candidate);
            }
            m_Stats.Culling.Add(VisibilityCullReason::Visible);
        }

        m_DrawListBuilder.Build(m_DrawCandidates.data(), static_cast<uint32_t>(m_DrawCandidates.size()), output);
        GpuDrawBuffers& buffers = outputBuffers != nullptr ? *outputBuffers : m_DrawBuffers;
        buffers.Upload(output);
        if (outputBuffers == nullptr)
        {
            m_Stats.VisibleInstances = m_Stats.Culling.Visible;
            m_Stats.IndirectCommands = static_cast<uint32_t>(output.Commands.size());
            m_Stats.IndirectRuns = static_cast<uint32_t>(output.Runs.size());
        }
        else
            m_Stats.Culling = previousCulling;
    }

    bool GpuScene::EnsureInstanceCapacity(uint32_t requiredCapacity)
    {
        if (requiredCapacity <= m_Instances.size())
        {
            if (m_InstanceBuffer || !CanCreateGpuBuffers() || m_Instances.empty())
                return false;
            requiredCapacity = static_cast<uint32_t>(m_Instances.size());
        }
        const uint32_t capacity = NextCapacity(requiredCapacity, 1024u);
        m_Instances.resize(capacity);
        m_InstanceStates.resize(capacity);
        if (!CanCreateGpuBuffers())
            return false;

        GenericGpuBufferDesc desc;
        desc.ElementCount = capacity;
        desc.ElementSize = sizeof(RenderInstanceData);
        desc.Type = GpuBufferType::Structured;
        desc.Usage = BufferUsage::BU_LOADSTORE;
        m_InstanceBuffer = GenericGpuBuffer::Create(desc);
        return m_InstanceBuffer != nullptr;
    }

    bool GpuScene::EnsureLightCapacity(uint32_t requiredCapacity)
    {
        if (requiredCapacity <= m_Lights.size())
        {
            if (m_LightBuffer || !CanCreateGpuBuffers() || m_Lights.empty())
                return false;
            requiredCapacity = static_cast<uint32_t>(m_Lights.size());
        }
        const uint32_t capacity = NextCapacity(requiredCapacity, 256u);
        m_Lights.resize(capacity);
        m_LightStates.resize(capacity);
        if (!CanCreateGpuBuffers())
            return false;

        GenericGpuBufferDesc desc;
        desc.ElementCount = capacity;
        desc.ElementSize = sizeof(RenderLightData);
        desc.Type = GpuBufferType::Structured;
        desc.Usage = BufferUsage::BU_LOADSTORE;
        m_LightBuffer = GenericGpuBuffer::Create(desc);
        return m_LightBuffer != nullptr;
    }

    bool GpuScene::EnsureShadowLightCapacity(uint32_t requiredCapacity)
    {
        if (requiredCapacity == 0 || !CanCreateGpuBuffers())
            return false;
        const uint32_t requiredSize = requiredCapacity * sizeof(GpuShadowLightData);
        if (m_ShadowLightBuffer && m_ShadowLightBuffer->GetSize() >= requiredSize)
            return false;
        GenericGpuBufferDesc desc;
        desc.ElementCount = NextCapacity(requiredCapacity, 256u);
        desc.ElementSize = sizeof(GpuShadowLightData);
        desc.Type = GpuBufferType::Structured;
        desc.Usage = BufferUsage::BU_LOADSTORE;
        m_ShadowLightBuffer = GenericGpuBuffer::Create(desc);
        return m_ShadowLightBuffer != nullptr;
    }

    bool GpuScene::EnsureShadowViewCapacity(uint32_t requiredCapacity)
    {
        if (requiredCapacity == 0 || !CanCreateGpuBuffers())
            return false;
        const uint32_t requiredSize = requiredCapacity * sizeof(GpuShadowViewData);
        if (m_ShadowViewBuffer && m_ShadowViewBuffer->GetSize() >= requiredSize)
            return false;
        GenericGpuBufferDesc desc;
        desc.ElementCount = NextCapacity(requiredCapacity, 64u);
        desc.ElementSize = sizeof(GpuShadowViewData);
        desc.Type = GpuBufferType::Structured;
        desc.Usage = BufferUsage::BU_LOADSTORE;
        m_ShadowViewBuffer = GenericGpuBuffer::Create(desc);
        return m_ShadowViewBuffer != nullptr;
    }

    void GpuScene::ReleaseGeometryResource(uint32_t meshIndex)
    {
        if (meshIndex >= m_MeshResources.size())
            return;
        MeshResourceState& state = m_MeshResources[meshIndex];
        if (state.Lods)
            m_LodTableAllocator.Release(state.Lods);
        if (state.Meshlets)
            m_MeshletTableAllocator.Release(state.Meshlets);
        if (state.GeometryAllocation)
        {
            GeometryHeapPage* page = GetGeometryHeapPage(state.GeometryBinding);
            if (page != nullptr && page->Heap)
                page->Heap->Release(state.GeometryAllocation);
        }
        state.Lods = {};
        state.Meshlets = {};
        state.GeometryAllocation = {};
        state.GeometryBinding = 0;

        m_Geometry.Meshes.resize(std::max<size_t>(m_MeshResources.size(), 1u));
        m_MeshIndexBuffers.resize(m_Geometry.Meshes.size());
        m_Geometry.Meshes[meshIndex] = {};
        m_MeshIndexBuffers[meshIndex] = nullptr;
        m_DirtyMeshIndices.push_back(meshIndex);
    }

    void GpuScene::UpdateGeometryResource(uint32_t meshIndex)
    {
        if (meshIndex == 0 || meshIndex >= m_MeshResources.size() || !m_MeshResources[meshIndex].Resource)
            return;
        MeshResourceState& state = m_MeshResources[meshIndex];
        const Mesh& mesh = *state.Resource;
        const MeshGpuGeometry& geometry = mesh.GetGpuGeometry();
        const bool hasMeshlets = !geometry.IsEmpty() && !geometry.Meshlets.empty();
        const uint32_t lodCount = hasMeshlets ? static_cast<uint32_t>(geometry.Lods.size()) : 1u;
        const uint32_t meshletCount =
          hasMeshlets ? static_cast<uint32_t>(geometry.Meshlets.size()) : static_cast<uint32_t>(std::max<size_t>(mesh.GetSubMeshes().size(), 1u));
        state.Lods = m_LodTableAllocator.Allocate(lodCount);
        state.Meshlets = m_MeshletTableAllocator.Allocate(meshletCount);
        if (!state.Lods || !state.Meshlets)
        {
            if (state.Lods)
                m_LodTableAllocator.Release(state.Lods);
            if (state.Meshlets)
                m_MeshletTableAllocator.Release(state.Meshlets);
            state.Lods = {};
            state.Meshlets = {};
            return;
        }

        m_Geometry.Lods.resize(std::max<size_t>(m_Geometry.Lods.size(), state.Lods.First + state.Lods.Count));
        m_Geometry.Meshlets.resize(std::max<size_t>(m_Geometry.Meshlets.size(), state.Meshlets.First + state.Meshlets.Count));
        m_MeshIndexBuffers.resize(m_Geometry.Meshes.size());

        state.GeometryBinding = MakePerMeshGeometryBinding(meshIndex);
        GeometryAllocation allocation;
        const bool heapResident = TryMakeGeometryResident(mesh, geometry, hasMeshlets, state, allocation);
        const uint32_t vertexOffset = heapResident ? allocation.VertexOffset : 0u;
        const uint32_t firstIndex = heapResident ? allocation.FirstIndex : 0u;
        GpuMeshRecord& record = m_Geometry.Meshes[meshIndex];
        record.LodRangeAndHeaps = { state.Lods.First, lodCount, state.GeometryBinding, state.GeometryBinding };
        record.GeometryOffsets = { 0u, 0u, state.Meshlets.First, meshletCount };

        if (hasMeshlets)
        {
            for (uint32_t lodIndex = 0; lodIndex < lodCount; lodIndex++)
            {
                const MeshLod& source = geometry.Lods[lodIndex];
                m_Geometry.Lods[state.Lods.First + lodIndex] = { state.Meshlets.First + source.FirstMeshlet, source.MeshletCount, source.Error, 0u };
            }
            uint32_t lodIndex = 0;
            for (uint32_t sourceIndex = 0; sourceIndex < meshletCount; sourceIndex++)
            {
                while (lodIndex + 1u < geometry.Lods.size() &&
                       sourceIndex >= geometry.Lods[lodIndex].FirstMeshlet + geometry.Lods[lodIndex].MeshletCount)
                    lodIndex++;
                const Meshlet& source = geometry.Meshlets[sourceIndex];
                GpuMeshletData& meshlet = m_Geometry.Meshlets[state.Meshlets.First + sourceIndex];
                meshlet.BoundingSphere = source.BoundingSphere;
                meshlet.NormalCone = source.NormalCone;
                meshlet.Draw = { firstIndex + source.TriangleOffset, source.TriangleCount * 3u, source.MaterialSlot, lodIndex };
                meshlet.Geometry = { vertexOffset, state.GeometryBinding, state.GeometryBinding, 0u };
            }
            if (!heapResident && CanCreateGpuBuffers() && !geometry.MeshletIndices.empty())
            {
                IndexBufferDesc desc;
                desc.Count = static_cast<uint32_t>(geometry.MeshletIndices.size());
                desc.Type = IndexType::Index_32;
                desc.Usage = BufferUsage::BU_STATIC_DRAW;
                desc.Data = geometry.MeshletIndices.data();
                m_MeshIndexBuffers[meshIndex] = IndexBuffer::Create(desc);
            }
            else
                m_MeshIndexBuffers[meshIndex] = heapResident ? nullptr : mesh.GetIndexBuffer();
        }
        else
        {
            const Vector<SubMesh>& subMeshes = mesh.GetSubMeshes();
            m_Geometry.Lods[state.Lods.First] = { state.Meshlets.First, meshletCount, 0.0f, 0u };
            const SphereBounds& bounds = mesh.GetSphereBounds();
            for (uint32_t subMeshIndex = 0; subMeshIndex < meshletCount; subMeshIndex++)
            {
                GpuMeshletData& meshlet = m_Geometry.Meshlets[state.Meshlets.First + subMeshIndex];
                meshlet.BoundingSphere = glm::vec4(bounds.GetCenter(), bounds.GetRadius());
                meshlet.NormalCone = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
                if (subMeshes.empty())
                    meshlet.Draw = { firstIndex, mesh.GetIndexCount(), 0u, 0u };
                else
                {
                    const SubMesh& source = subMeshes[subMeshIndex];
                    meshlet.Draw = { firstIndex + source.IndexOffset, source.IndexCount, subMeshIndex, 0u };
                }
                meshlet.Geometry = { vertexOffset, state.GeometryBinding, state.GeometryBinding, 0u };
            }
            m_MeshIndexBuffers[meshIndex] = heapResident ? nullptr : mesh.GetIndexBuffer();
        }

        m_MeshLodRanges.push_back({ state.Lods.First, state.Lods.Count });
        m_MeshletRanges.push_back({ state.Meshlets.First, state.Meshlets.Count });
    }

    void GpuScene::FlushGeometryTables()
    {
        BuildDirtyRanges(m_DirtyMeshIndices, m_MeshRanges);
        MergeDirtyRanges(m_MeshLodRanges);
        MergeDirtyRanges(m_MeshletRanges);

        uint32_t meshCapacity = m_Stats.MeshCapacity;
        uint32_t lodCapacity = m_Stats.MeshLodCapacity;
        uint32_t meshletCapacity = m_Stats.MeshletCapacity;
        UploadTableRanges(m_MeshBuffer, m_Geometry.Meshes.data(), static_cast<uint32_t>(m_Geometry.Meshes.size()), sizeof(GpuMeshRecord), 64u,
                          meshCapacity, m_MeshRanges, m_Stats.MeshRanges);
        UploadTableRanges(m_MeshLodBuffer, m_Geometry.Lods.data(), static_cast<uint32_t>(m_Geometry.Lods.size()), sizeof(GpuMeshLodData), 128u,
                          lodCapacity, m_MeshLodRanges, m_Stats.MeshRanges);
        UploadTableRanges(m_MeshletBuffer, m_Geometry.Meshlets.data(), static_cast<uint32_t>(m_Geometry.Meshlets.size()), sizeof(GpuMeshletData),
                          1024u, meshletCapacity, m_MeshletRanges, m_Stats.MeshRanges);
        m_Stats.MeshCapacity = CanCreateGpuBuffers() ? meshCapacity : static_cast<uint32_t>(m_Geometry.Meshes.size());
        m_Stats.MeshLodCapacity = CanCreateGpuBuffers() ? lodCapacity : static_cast<uint32_t>(m_Geometry.Lods.size());
        m_Stats.MeshletCapacity = CanCreateGpuBuffers() ? meshletCapacity : static_cast<uint32_t>(m_Geometry.Meshlets.size());
        const GpuGeometryTableAllocatorStats lodStats = m_LodTableAllocator.GetStats();
        const GpuGeometryTableAllocatorStats meshletStats = m_MeshletTableAllocator.GetStats();
        m_Stats.LiveMeshLods = lodStats.LiveElements;
        m_Stats.LiveMeshlets = meshletStats.LiveElements;
        m_Stats.RetiredMeshMetadata = lodStats.RetiredElements + meshletStats.RetiredElements;
    }

    bool GpuScene::TryMakeGeometryResident(const Mesh& mesh, const MeshGpuGeometry& geometry, bool hasMeshlets, MeshResourceState& state,
                                           GeometryAllocation& allocation)
    {
        if (!CanUseStaticGeometryHeaps() || mesh.IsDynamic() || mesh.GetMorph() || mesh.GetSkeleton() || !mesh.GetVertexBuffer() ||
            !mesh.GetIndexBuffer() || !mesh.GetVertexBuffer()->GetLayout() || mesh.GetVertexLayout().GetStreamCount() != 1u)
            return false;

        const IndexType indexType = hasMeshlets ? IndexType::Index_32 : mesh.GetIndexType();
        const uint32_t indexCount = hasMeshlets ? static_cast<uint32_t>(geometry.MeshletIndices.size()) : mesh.GetIndexCount();
        const uint64_t vertexSize64 = static_cast<uint64_t>(mesh.GetVertexCount()) * mesh.GetVertexLayout().GetStride();
        if (indexCount == 0 || vertexSize64 == 0 || vertexSize64 > std::numeric_limits<uint32_t>::max())
            return false;
        const uint32_t vertexSize = static_cast<uint32_t>(vertexSize64);
        GeometryHeapPage* page = AllocateGeometryHeapPage(mesh, indexType, vertexSize, indexCount, allocation);
        if (page == nullptr)
        {
            m_GeometryResidencyFailures++;
            return false;
        }
        if (!page->Heap->CopyVertices(allocation.Handle, *mesh.GetVertexBuffer()))
        {
            page->Heap->Release(allocation.Handle);
            allocation = {};
            m_GeometryResidencyFailures++;
            return false;
        }
        const bool indicesUploaded = hasMeshlets ? page->Heap->UploadIndices(allocation.Handle, geometry.MeshletIndices.data())
                                                 : page->Heap->CopyIndices(allocation.Handle, *mesh.GetIndexBuffer());
        if (!indicesUploaded)
        {
            page->Heap->Release(allocation.Handle);
            allocation = {};
            m_GeometryResidencyFailures++;
            return false;
        }

        state.GeometryAllocation = allocation.Handle;
        state.GeometryBinding = page->Binding;
        const uint64_t indexBytes = static_cast<uint64_t>(indexCount) * (indexType == IndexType::Index_16 ? sizeof(uint16_t) : sizeof(uint32_t));
        const uint64_t uploadBytes = vertexSize + indexBytes;
        m_Stats.GeometryUploadBytes += uploadBytes;
        m_Stats.UploadedBytes += uploadBytes;
        return true;
    }

    GpuScene::GeometryHeapPage* GpuScene::AllocateGeometryHeapPage(const Mesh& mesh, IndexType indexType, uint32_t vertexSizeBytes,
                                                                   uint32_t indexCount, GeometryAllocation& allocation)
    {
        const Ref<BufferLayout>& layout = mesh.GetVertexBuffer()->GetLayout();
        const uint32_t indexSize = indexType == IndexType::Index_16 ? sizeof(uint16_t) : sizeof(uint32_t);
        for (GeometryHeapPage& page : m_GeometryHeapPages)
        {
            if (page.Indices != indexType || page.Topology != mesh.GetDrawMode() || !GeometryLayoutsMatch(*page.Layout, *layout))
                continue;
            const GeometryHeapStats stats = page.Heap->GetStats();
            if (stats.LargestFreeVertexRange < vertexSizeBytes || stats.LargestFreeIndexRange < static_cast<uint64_t>(indexCount) * indexSize)
                continue;
            if (page.Heap->Allocate(vertexSizeBytes, indexCount, allocation))
                return &page;
        }

        const uint32_t vertexCapacity = GeometryPageCapacity(vertexSizeBytes, MinimumGeometryVertexCapacity, MaximumGeometryVertexCapacity);
        const uint32_t indexCapacity = GeometryPageCapacity(indexCount, MinimumGeometryIndexCapacity, MaximumGeometryIndexCapacity);
        if (vertexCapacity == 0 || indexCapacity == 0 || m_GeometryHeapPages.size() + 1u >= PerMeshGeometryBindingBit)
            return nullptr;

        GeometryHeapDesc desc;
        desc.VertexCapacityBytes = vertexCapacity;
        desc.IndexCapacity = indexCapacity;
        desc.VertexStride = layout->GetStride();
        desc.Indices = indexType;
        desc.FramesInFlight = 2;
        GeometryHeapPage page;
        page.Binding = static_cast<uint32_t>(m_GeometryHeapPages.size()) + 1u;
        page.Topology = mesh.GetDrawMode();
        page.Indices = indexType;
        page.Layout = layout;
        page.Heap = CreateScope<StaticGeometryHeap>(desc);
        if (!page.Heap->InitializeGpuBuffers(layout) || !page.Heap->Allocate(vertexSizeBytes, indexCount, allocation))
            return nullptr;
        m_GeometryHeapPages.push_back(std::move(page));
        return &m_GeometryHeapPages.back();
    }

    GpuScene::GeometryHeapPage* GpuScene::GetGeometryHeapPage(uint32_t geometryBinding)
    {
        if (geometryBinding == 0 || IsPerMeshGeometryBinding(geometryBinding) || geometryBinding > m_GeometryHeapPages.size())
            return nullptr;
        return &m_GeometryHeapPages[geometryBinding - 1u];
    }

    const GpuScene::GeometryHeapPage* GpuScene::GetGeometryHeapPage(uint32_t geometryBinding) const
    {
        if (geometryBinding == 0 || IsPerMeshGeometryBinding(geometryBinding) || geometryBinding > m_GeometryHeapPages.size())
            return nullptr;
        return &m_GeometryHeapPages[geometryBinding - 1u];
    }

    void GpuScene::UpdateGeometryHeapStats()
    {
        m_Stats.GeometryHeapPages = static_cast<uint32_t>(m_GeometryHeapPages.size());
        m_Stats.GeometryHeapCapacityBytes = 0;
        m_Stats.GeometryHeapLiveBytes = 0;
        m_Stats.GeometryHeapHighWaterBytes = 0;
        m_Stats.GeometryHeapRetiredAllocations = 0;
        m_Stats.GeometryHeapFailedAllocations = m_GeometryResidencyFailures;
        for (const GeometryHeapPage& page : m_GeometryHeapPages)
        {
            const GeometryHeapStats stats = page.Heap->GetStats();
            m_Stats.GeometryHeapCapacityBytes += stats.VertexCapacityBytes + stats.IndexCapacityBytes;
            m_Stats.GeometryHeapLiveBytes += stats.LiveBytes;
            m_Stats.GeometryHeapHighWaterBytes += stats.HighWaterBytes;
            m_Stats.GeometryHeapRetiredAllocations += stats.RetiredAllocations;
            m_Stats.GeometryHeapFailedAllocations += stats.FailedAllocations;
        }
    }

    uint32_t GpuScene::MakePerMeshGeometryBinding(uint32_t meshIndex)
    {
        return meshIndex < PerMeshGeometryBindingBit ? PerMeshGeometryBindingBit | meshIndex : 0u;
    }

    bool GpuScene::IsPerMeshGeometryBinding(uint32_t geometryBinding) { return (geometryBinding & PerMeshGeometryBindingBit) != 0; }

    uint32_t GpuScene::GetPerMeshGeometryIndex(uint32_t geometryBinding) { return geometryBinding & ~PerMeshGeometryBindingBit; }

    void GpuScene::RebuildMaterialTable()
    {
        m_Materials.clear();
        m_Materials.resize(std::max<size_t>(m_MaterialResources.size(), 1u));
        const Ref<Texture> fallback = Texture::MISSING ? Texture::MISSING : Texture::WHITE;
        const uint32_t capacity = std::min<uint32_t>(BindlessResourceHandle::MaxResources,
                                                     std::max<uint32_t>(1024u, static_cast<uint32_t>(m_MaterialResources.size() * 5u + 4u)));
        m_BindlessTextures = CreateScope<BindlessResourceTable>(capacity, reinterpret_cast<uint64_t>(fallback.get()));
        m_BindlessTextureResources.clear();
        m_BindlessTextureResources.resize(1u, fallback);
        UnorderedMap<const Texture*, uint32_t> textureIndices;
        if (fallback)
            textureIndices.emplace(fallback.get(), 0u);

        auto registerTexture = [&](const Ref<Texture>& texture, const Ref<Texture>& defaultTexture) {
            const Ref<Texture> resolved = texture ? texture : defaultTexture;
            if (!resolved)
                return 0u;
            const auto existing = textureIndices.find(resolved.get());
            if (existing != textureIndices.end())
                return existing->second;
            const BindlessResourceHandle handle = m_BindlessTextures->Allocate(reinterpret_cast<uint64_t>(resolved.get()));
            if (!handle)
                return 0u;
            const uint32_t index = handle.GetIndex();
            if (index >= m_BindlessTextureResources.size())
                m_BindlessTextureResources.resize(index + 1u);
            m_BindlessTextureResources[index] = resolved;
            textureIndices.emplace(resolved.get(), index);
            return index;
        };

        for (uint32_t materialIndex = 1; materialIndex < m_MaterialResources.size(); materialIndex++)
        {
            const AssetHandle<Material>& materialHandle = m_MaterialResources[materialIndex].Resource;
            if (!materialHandle)
                continue;
            const Material& material = *materialHandle;
            StandardMaterialDesc desc;
            ReadMaterialValue(material, { "baseColor", "albedo", "tint" }, desc.BaseColor);
            if (!ReadMaterialValue(material, { "emissive", "emissionColor" }, desc.Emissive))
            {
                glm::vec4 emissive(0.0f);
                if (ReadMaterialValue(material, { "emissive", "emissionColor" }, emissive))
                    desc.Emissive = glm::vec3(emissive);
            }
            ReadMaterialValue(material, { "emissiveIntensity", "emissionIntensity" }, desc.EmissiveIntensity);
            ReadMaterialValue(material, { "alphaCutoff", "cutoff" }, desc.AlphaCutoff);
            ReadMaterialValue(material, { "metallic", "metalness" }, desc.Metallic);
            ReadMaterialValue(material, { "roughness" }, desc.Roughness);
            ReadMaterialValue(material, { "normalScale", "normalStrength" }, desc.NormalScale);
            ReadMaterialValue(material, { "ambientOcclusion", "ao" }, desc.AmbientOcclusion);

            const AssetHandle<Shader> shader = material.GetShader();
            if (shader && shader->GetName().find("Unlit") != String::npos)
                desc.Model = MaterialModel::Unlit;
            else if ((shader && shader->GetName().find("Toon") != String::npos) ||
                     (material.GetVariation().Has("TOON") && material.GetVariation().GetBool("TOON")))
                desc.Model = MaterialModel::Toon;
            if (shader)
            {
                const Ref<ShaderTechnique>& technique = shader->GetTechnique(material.GetVariation());
                if (technique && !technique->GetRenderPasses().empty() && technique->GetRenderPasses()[0]->HasBlending())
                    desc.Alpha = AlphaMode::Premultiplied;
            }
            if (material.GetVariation().Has("ALPHA_MASK") && material.GetVariation().GetBool("ALPHA_MASK"))
                desc.Alpha = AlphaMode::Mask;

            desc.BaseColorTexture =
              registerTexture(FindMaterialTexture(material, { "baseColorTexture", "baseColorMap", "albedoMap", "mainTexture" }), Texture::WHITE);
            desc.NormalTexture = registerTexture(FindMaterialTexture(material, { "normalTexture", "normalMap" }), Texture::NORMAL);
            desc.MetallicRoughnessTexture =
              registerTexture(FindMaterialTexture(material, { "metallicRoughnessTexture", "metallicRoughnessMap", "metallicMap" }), Texture::WHITE);
            desc.AmbientOcclusionTexture =
              registerTexture(FindMaterialTexture(material, { "ambientOcclusionTexture", "ambientOcclusionMap", "aoMap" }), Texture::WHITE);
            desc.EmissiveTexture =
              registerTexture(FindMaterialTexture(material, { "emissiveTexture", "emissiveMap", "emissionMap" }), Texture::BLACK);
            if (desc.Model == MaterialModel::Toon)
            {
                glm::vec4 color(desc.ToonShadowColor, 1.0f);
                if (ReadMaterialValue(material, { "toonShadowColor", "shadowColor", "shadowTint" }, color))
                    desc.ToonShadowColor = glm::vec3(color);
                color = glm::vec4(desc.ToonSpecularColor, 1.0f);
                if (ReadMaterialValue(material, { "toonSpecularColor", "specularColor" }, color))
                    desc.ToonSpecularColor = glm::vec3(color);
                color = glm::vec4(desc.ToonRimColor, 1.0f);
                if (ReadMaterialValue(material, { "toonRimColor", "rimColor" }, color))
                    desc.ToonRimColor = glm::vec3(color);
                ReadMaterialValue(material, { "toonBands", "bands" }, desc.ToonBands);
                ReadMaterialValue(material, { "toonBandSmoothness", "bandSmoothness", "stepSmoothness" }, desc.ToonBandSmoothness);
                ReadMaterialValue(material, { "toonSpecularThreshold", "specularThreshold", "specularSize" }, desc.ToonSpecularThreshold);
                ReadMaterialValue(material, { "toonSpecularSmoothness", "specularSmoothness" }, desc.ToonSpecularSmoothness);
                ReadMaterialValue(material, { "toonSpecularStrength", "specularStrength" }, desc.ToonSpecularStrength);
                ReadMaterialValue(material, { "toonRimThreshold", "rimThreshold", "rimWidth" }, desc.ToonRimThreshold);
                ReadMaterialValue(material, { "toonRimSmoothness", "rimSmoothness" }, desc.ToonRimSmoothness);
                ReadMaterialValue(material, { "toonRimPower", "rimPower" }, desc.ToonRimPower);
                ReadMaterialValue(material, { "toonRimStrength", "rimStrength" }, desc.ToonRimStrength);
                ReadMaterialValue(material, { "toonRimShadowMask", "rimShadowMask" }, desc.ToonRimShadowMask);
                ReadMaterialValue(material, { "toonIndirectStrength", "indirectStrength" }, desc.ToonIndirectStrength);
                ReadMaterialValue(material, { "toonPatternScale", "patternScale", "patternTiling" }, desc.ToonPatternScale);
                ReadMaterialValue(material, { "toonPatternStrength", "patternStrength", "patternAmount" }, desc.ToonPatternStrength);
                ReadMaterialValue(material, { "toonPatternSmoothness", "patternSmoothness" }, desc.ToonPatternSmoothness);
                ReadMaterialValue(material, { "toonPatternDistanceFade", "patternDistanceFade" }, desc.ToonPatternDistanceFade);
                ReadMaterialValue(material, { "toonRampStrength", "rampStrength" }, desc.ToonRampStrength);
                ReadMaterialValue(material, { "toonRampOffset", "rampOffset" }, desc.ToonRampOffset);
                ReadMaterialValue(material, { "toonMatcapStrength", "matcapStrength" }, desc.ToonMatcapStrength);
                ReadMaterialValue(material, { "toonMatcapRotation", "matcapRotation" }, desc.ToonMatcapRotation);
                int patternMapping = static_cast<int>(desc.ToonPatternMappingMode);
                if (ReadMaterialValue(material, { "toonPatternMapping", "patternMapping", "patternUvMode" }, patternMapping))
                    desc.ToonPatternMappingMode = static_cast<ToonPatternMapping>(glm::clamp(patternMapping, 0, 2));
                color = desc.ToonOutlineColor;
                if (ReadMaterialValue(material, { "toonOutlineColor", "outlineColor" }, color))
                    desc.ToonOutlineColor = color;
                ReadMaterialValue(material, { "toonOutlineWidth", "outlineWidth", "thickness" }, desc.ToonOutlineWidth);
                ReadMaterialValue(material, { "toonOutlineDepthThreshold", "outlineDepthThreshold" }, desc.ToonOutlineDepthThreshold);
                ReadMaterialValue(material, { "toonOutlineNormalThreshold", "outlineNormalThreshold" }, desc.ToonOutlineNormalThreshold);
                ReadMaterialValue(material, { "toonOutlineDistanceFade", "outlineDistanceFade" }, desc.ToonOutlineDistanceFade);
                desc.ToonPatternTexture = registerTexture(
                  FindMaterialTexture(material, { "toonPatternTexture", "patternTexture", "hatchingTexture", "scratchTexture" }), Texture::WHITE);
                desc.ToonRampTexture =
                  registerTexture(FindMaterialTexture(material, { "toonRampTexture", "rampTexture", "diffuseRamp" }), Texture::WHITE);
                desc.ToonMatcapTexture =
                  registerTexture(FindMaterialTexture(material, { "toonMatcapTexture", "matcapTexture", "matcap" }), Texture::WHITE);
            }
            m_Materials[materialIndex] = GpuMaterialPacker::Pack(desc);
        }

        uint32_t materialCapacity = m_Stats.MaterialCapacity;
        UploadTable(m_MaterialBuffer, m_Materials.data(), static_cast<uint32_t>(m_Materials.size()), sizeof(GpuMaterialData), 256u, materialCapacity,
                    m_Stats.MaterialRanges);
        m_Stats.MaterialCapacity = CanCreateGpuBuffers() ? materialCapacity : static_cast<uint32_t>(m_Materials.size());
        m_Stats.BindlessTextureCount = static_cast<uint32_t>(m_BindlessTextureResources.size());
        m_BindlessTextureVersion++;
        if (m_BindlessTextureVersion == 0)
            m_BindlessTextureVersion = 1;
    }

    void GpuScene::UploadTable(Ref<GenericGpuBuffer>& buffer, const void* data, uint32_t elementCount, uint32_t elementSize, uint32_t minimumCapacity,
                               uint32_t& capacity, uint32_t& rangeCount)
    {
        if (!CanCreateGpuBuffers())
            return;
        const uint32_t required = std::max(elementCount, 1u);
        BufferWriteOptions writeOptions = BWT_NORMAL;
        if (!buffer || required > capacity)
        {
            capacity = NextCapacity(required, minimumCapacity);
            GenericGpuBufferDesc desc;
            desc.ElementCount = capacity;
            desc.ElementSize = elementSize;
            desc.Type = GpuBufferType::Structured;
            desc.Usage = BufferUsage::BU_LOADSTORE;
            buffer = GenericGpuBuffer::Create(desc);
            writeOptions = BWT_DISCARD;
        }
        if (!buffer || data == nullptr || elementCount == 0)
            return;
        const uint32_t size = elementCount * elementSize;
        buffer->WriteData(0, size, data, writeOptions);
        m_Stats.UploadedBytes += size;
        rangeCount++;
    }

    void GpuScene::UploadTableRanges(Ref<GenericGpuBuffer>& buffer, const void* data, uint32_t elementCount, uint32_t elementSize,
                                     uint32_t minimumCapacity, uint32_t& capacity, Vector<DirtyRange>& ranges, uint32_t& rangeCount)
    {
        if (!CanCreateGpuBuffers() || elementCount == 0 || data == nullptr)
            return;
        const uint32_t required = std::max(elementCount, 1u);
        if (!buffer || required > capacity)
        {
            capacity = NextCapacity(required, minimumCapacity);
            GenericGpuBufferDesc desc;
            desc.ElementCount = capacity;
            desc.ElementSize = elementSize;
            desc.Type = GpuBufferType::Structured;
            desc.Usage = BufferUsage::BU_LOADSTORE;
            buffer = GenericGpuBuffer::Create(desc);
            if (!buffer)
                return;
            const uint64_t size = static_cast<uint64_t>(elementCount) * elementSize;
            if (size > std::numeric_limits<uint32_t>::max())
                return;
            buffer->WriteData(0, static_cast<uint32_t>(size), data, BWT_DISCARD);
            m_Stats.UploadedBytes += size;
            rangeCount++;
            return;
        }

        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        for (const DirtyRange& range : ranges)
        {
            if (range.Count == 0 || range.First >= elementCount || range.Count > elementCount - range.First)
                continue;
            const uint64_t offset = static_cast<uint64_t>(range.First) * elementSize;
            const uint64_t size = static_cast<uint64_t>(range.Count) * elementSize;
            if (offset > std::numeric_limits<uint32_t>::max() || size > std::numeric_limits<uint32_t>::max())
                continue;
            buffer->WriteData(static_cast<uint32_t>(offset), static_cast<uint32_t>(size), bytes + offset, BWT_NORMAL);
            m_Stats.UploadedBytes += size;
            rangeCount++;
        }
    }

    void GpuScene::BuildDirtyRanges(Vector<uint32_t>& indices, Vector<DirtyRange>& ranges)
    {
        ranges.clear();
        if (indices.empty())
            return;
        std::sort(indices.begin(), indices.end());
        indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
        DirtyRange range{ indices[0], 1u };
        for (uint32_t index = 1; index < indices.size(); index++)
        {
            if (indices[index] == range.First + range.Count)
            {
                range.Count++;
                continue;
            }
            ranges.push_back(range);
            range = { indices[index], 1u };
        }
        ranges.push_back(range);
    }

    void GpuScene::MergeDirtyRanges(Vector<DirtyRange>& ranges)
    {
        ranges.erase(std::remove_if(ranges.begin(), ranges.end(), [](const DirtyRange& range) { return range.Count == 0; }), ranges.end());
        if (ranges.empty())
            return;
        std::sort(ranges.begin(), ranges.end(), [](const DirtyRange& first, const DirtyRange& second) { return first.First < second.First; });
        size_t output = 0;
        for (size_t index = 1; index < ranges.size(); index++)
        {
            DirtyRange& current = ranges[output];
            const DirtyRange& next = ranges[index];
            const uint64_t currentEnd = static_cast<uint64_t>(current.First) + current.Count;
            const uint64_t nextEnd = static_cast<uint64_t>(next.First) + next.Count;
            if (next.First <= currentEnd)
                current.Count = static_cast<uint32_t>(std::max(currentEnd, nextEnd) - current.First);
            else
                ranges[++output] = next;
        }
        ranges.resize(output + 1u);
    }

    void GpuScene::FlushInstanceRanges(const Vector<DirtyRange>& ranges)
    {
        if (!m_InstanceBuffer)
            return;
        for (const DirtyRange& range : ranges)
        {
            const uint32_t offset = range.First * sizeof(RenderInstanceData);
            const uint32_t size = range.Count * sizeof(RenderInstanceData);
            m_InstanceBuffer->WriteData(offset, size, m_Instances.data() + range.First, BWT_NORMAL);
            m_Stats.UploadedBytes += size;
        }
        m_Stats.InstanceRanges += static_cast<uint32_t>(ranges.size());
    }

    void GpuScene::FlushLightRanges(const Vector<DirtyRange>& ranges)
    {
        if (!m_LightBuffer)
            return;
        for (const DirtyRange& range : ranges)
        {
            const uint32_t offset = range.First * sizeof(RenderLightData);
            const uint32_t size = range.Count * sizeof(RenderLightData);
            m_LightBuffer->WriteData(offset, size, m_Lights.data() + range.First, BWT_NORMAL);
            m_Stats.UploadedBytes += size;
        }
        m_Stats.LightRanges += static_cast<uint32_t>(ranges.size());
    }

    bool GpuScene::CanCreateGpuBuffers() const
    {
        return m_EnableGpuBuffers && RenderAPI::TryGet() != nullptr && RenderAPI::TryGet()->GetCapabilities().HasCapability(CW_LOAD_STORE);
    }

    bool GpuScene::CanUseStaticGeometryHeaps() const { return CanCreateGpuBuffers() && RenderAPI::GetAPI() == RenderAPI::API::Vulkan; }
} // namespace Crowny
