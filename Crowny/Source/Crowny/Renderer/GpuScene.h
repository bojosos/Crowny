#pragma once

#include "Crowny/Common/Types.h"
#include "Crowny/Memory/FrameVector.h"
#include "Crowny/RenderAPI/GenericGpuBuffer.h"
#include "Crowny/RenderAPI/IndexBuffer.h"
#include "Crowny/Renderer/BindlessResourceTable.h"
#include "Crowny/Renderer/GeometryHeap.h"
#include "Crowny/Renderer/GpuDrivenDraw.h"
#include "Crowny/Renderer/GpuGeometry.h"
#include "Crowny/Renderer/GpuMaterial.h"
#include "Crowny/Renderer/RenderLight.h"
#include "Crowny/Renderer/RenderResourceChanges.h"
#include "Crowny/Renderer/RenderWorld.h"
#include "Crowny/Renderer/ShadowGpuData.h"
#include "Crowny/Renderer/VisibilityCulling.h"

namespace Crowny
{
    struct GpuSceneUploadStats
    {
        uint64_t UploadedBytes = 0;
        uint32_t InstanceRanges = 0;
        uint32_t LightRanges = 0;
        uint32_t ShadowRanges = 0;
        uint32_t MeshRanges = 0;
        uint32_t MaterialRanges = 0;
        uint32_t ActiveInstances = 0;
        uint32_t ActiveLights = 0;
        uint32_t InstanceCapacity = 0;
        uint32_t LightCapacity = 0;
        uint32_t ShadowLightCapacity = 0;
        uint32_t ShadowViewCapacity = 0;
        uint32_t MeshCapacity = 0;
        uint32_t MeshLodCapacity = 0;
        uint32_t MeshletCapacity = 0;
        uint32_t LiveMeshLods = 0;
        uint32_t LiveMeshlets = 0;
        uint32_t RetiredMeshMetadata = 0;
        uint32_t MaterialCapacity = 0;
        uint32_t BindlessTextureCount = 0;
        uint32_t VisibleInstances = 0;
        uint32_t IndirectCommands = 0;
        uint32_t IndirectRuns = 0;
        uint32_t GeometryHeapPages = 0;
        uint64_t GeometryHeapCapacityBytes = 0;
        uint64_t GeometryHeapLiveBytes = 0;
        uint64_t GeometryHeapHighWaterBytes = 0;
        uint64_t GeometryHeapRetiredAllocations = 0;
        uint64_t GeometryHeapFailedAllocations = 0;
        uint64_t GeometryUploadBytes = 0;
        VisibilityCullingStats Culling;
    };

    // Owns the sparse CPU shadow and persistent GPU tables consumed by culling,
    // shadow, Forward+, and Deferred+ passes. Stable frames perform no writes.
    class GpuScene
    {
    public:
        explicit GpuScene(bool enableGpuBuffers = true);

        void BeginFrame(uint64_t frameNumber);
        void Apply(const RenderWorldChange* instanceChanges, uint32_t instanceChangeCount, const RenderLightChange* lightChanges,
                   uint32_t lightChangeCount);
        void Apply(const FrameVector<RenderWorldChange>& instanceChanges, const FrameVector<RenderLightChange>& lightChanges)
        {
            Apply(instanceChanges.begin(), static_cast<uint32_t>(instanceChanges.Size()), lightChanges.begin(),
                  static_cast<uint32_t>(lightChanges.Size()));
        }
        void ApplyResources(const RenderMeshResourceChange* meshChanges, uint32_t meshChangeCount,
                            const RenderMaterialResourceChange* materialChanges, uint32_t materialChangeCount);
        void ApplyResources(const FrameVector<RenderMeshResourceChange>& meshChanges,
                            const FrameVector<RenderMaterialResourceChange>& materialChanges)
        {
            ApplyResources(meshChanges.begin(), static_cast<uint32_t>(meshChanges.Size()), materialChanges.begin(),
                           static_cast<uint32_t>(materialChanges.Size()));
        }

        void Reset();
        void UploadShadowData(const GpuShadowLightData* lights, uint32_t lightCount, const GpuShadowViewData* views, uint32_t viewCount);
        const Ref<GenericGpuBuffer>& GetInstanceBuffer() const { return m_InstanceBuffer; }
        const Ref<GenericGpuBuffer>& GetLightBuffer() const { return m_LightBuffer; }
        const Ref<GenericGpuBuffer>& GetShadowLightBuffer() const { return m_ShadowLightBuffer; }
        const Ref<GenericGpuBuffer>& GetShadowViewBuffer() const { return m_ShadowViewBuffer; }
        const Ref<GenericGpuBuffer>& GetMeshBuffer() const { return m_MeshBuffer; }
        const Ref<GenericGpuBuffer>& GetMeshLodBuffer() const { return m_MeshLodBuffer; }
        const Ref<GenericGpuBuffer>& GetMeshletBuffer() const { return m_MeshletBuffer; }
        const Ref<GenericGpuBuffer>& GetMaterialBuffer() const { return m_MaterialBuffer; }
        Ref<VertexBuffer> GetGeometryVertexBuffer(uint32_t geometryBinding) const;
        Ref<IndexBuffer> GetGeometryIndexBuffer(uint32_t geometryBinding) const;
        DrawMode GetGeometryDrawMode(uint32_t geometryBinding) const;
        const AssetHandle<Mesh>& GetMeshResource(uint32_t meshIndex) const;
        const AssetHandle<Material>& GetMaterialResource(uint32_t materialIndex) const;
        const Vector<Ref<Texture>>& GetBindlessTextures() const { return m_BindlessTextureResources; }
        uint64_t GetBindlessTextureVersion() const { return m_BindlessTextureVersion; }
        void DrainBindlessTextureUpdates(Vector<BindlessResourceUpdate>& output);
        void BuildCpuDrawList(const RenderView& view, GpuDrawList& output, GpuDrawBuffers* outputBuffers = nullptr, bool shadowCastersOnly = false);
        const GpuDrawBuffers& GetCpuDrawBuffers() const { return m_DrawBuffers; }
        const GpuSceneUploadStats& GetStats() const { return m_Stats; }
        bool HasGpuBuffers() const { return m_InstanceBuffer != nullptr && m_LightBuffer != nullptr; }

        bool TryGetInstance(RenderInstanceHandle handle, RenderInstanceData& output) const;
        bool TryGetLight(RenderLightHandle handle, RenderLightData& output) const;
        uint32_t GetShadowLightCount() const { return static_cast<uint32_t>(m_ShadowLights.size()); }
        uint32_t GetShadowViewCount() const { return static_cast<uint32_t>(m_ShadowViews.size()); }

    private:
        struct SlotState
        {
            uint32_t Generation = 0;
            bool Alive = false;
        };

        struct DirtyRange
        {
            uint32_t First = 0;
            uint32_t Count = 0;
        };

        struct MeshResourceState
        {
            AssetHandle<Mesh> Resource;
            uint64_t Version = 0;
            GpuGeometryTableRange Lods;
            GpuGeometryTableRange Meshlets;
            GeometryAllocationHandle GeometryAllocation;
            uint32_t GeometryBinding = 0;
        };

        struct MaterialResourceState
        {
            AssetHandle<Material> Resource;
            uint64_t Version = 0;
        };

        struct GeometryHeapPage
        {
            uint32_t Binding = 0;
            DrawMode Topology = DrawMode::TRIANGLE_LIST;
            IndexType Indices = IndexType::Index_32;
            Ref<BufferLayout> Layout;
            Scope<StaticGeometryHeap> Heap;
        };

        bool EnsureInstanceCapacity(uint32_t requiredCapacity);
        bool EnsureLightCapacity(uint32_t requiredCapacity);
        bool EnsureShadowLightCapacity(uint32_t requiredCapacity);
        bool EnsureShadowViewCapacity(uint32_t requiredCapacity);
        void UpdateGeometryResource(uint32_t meshIndex);
        void ReleaseGeometryResource(uint32_t meshIndex);
        void FlushGeometryTables();
        void RebuildMaterialTable();
        void UploadTable(Ref<GenericGpuBuffer>& buffer, const void* data, uint32_t elementCount, uint32_t elementSize, uint32_t minimumCapacity,
                         uint32_t& capacity, uint32_t& rangeCount);
        void UploadTableRanges(Ref<GenericGpuBuffer>& buffer, const void* data, uint32_t elementCount, uint32_t elementSize, uint32_t minimumCapacity,
                               uint32_t& capacity, Vector<DirtyRange>& ranges, uint32_t& rangeCount);
        void BuildDirtyRanges(Vector<uint32_t>& indices, Vector<DirtyRange>& ranges);
        void MergeDirtyRanges(Vector<DirtyRange>& ranges);
        void FlushInstanceRanges(const Vector<DirtyRange>& ranges);
        void FlushLightRanges(const Vector<DirtyRange>& ranges);
        bool TryMakeGeometryResident(const Mesh& mesh, const MeshGpuGeometry& geometry, bool hasMeshlets, MeshResourceState& state,
                                     GeometryAllocation& allocation);
        GeometryHeapPage* AllocateGeometryHeapPage(const Mesh& mesh, IndexType indexType, uint32_t vertexSizeBytes, uint32_t indexCount,
                                                   GeometryAllocation& allocation);
        GeometryHeapPage* GetGeometryHeapPage(uint32_t geometryBinding);
        const GeometryHeapPage* GetGeometryHeapPage(uint32_t geometryBinding) const;
        void UpdateGeometryHeapStats();
        bool CanCreateGpuBuffers() const;
        bool CanUseStaticGeometryHeaps() const;

        static constexpr uint32_t PerMeshGeometryBindingBit = 0x80000000u;
        static uint32_t MakePerMeshGeometryBinding(uint32_t meshIndex);
        static bool IsPerMeshGeometryBinding(uint32_t geometryBinding);
        static uint32_t GetPerMeshGeometryIndex(uint32_t geometryBinding);

        bool m_EnableGpuBuffers = true;
        Vector<RenderInstanceData> m_Instances;
        Vector<RenderLightData> m_Lights;
        Vector<GpuShadowLightData> m_ShadowLights;
        Vector<GpuShadowViewData> m_ShadowViews;
        Vector<SlotState> m_InstanceStates;
        Vector<SlotState> m_LightStates;
        Vector<MeshResourceState> m_MeshResources;
        Vector<MaterialResourceState> m_MaterialResources;
        Vector<GeometryHeapPage> m_GeometryHeapPages;
        uint64_t m_GeometryResidencyFailures = 0;
        PackedGpuGeometry m_Geometry;
        GpuGeometryTableAllocator m_LodTableAllocator;
        GpuGeometryTableAllocator m_MeshletTableAllocator;
        Vector<GpuMaterialData> m_Materials;
        Vector<Ref<IndexBuffer>> m_MeshIndexBuffers;
        Vector<Ref<Texture>> m_BindlessTextureResources;
        uint64_t m_BindlessTextureVersion = 0;
        Scope<BindlessResourceTable> m_BindlessTextures;
        Vector<GpuDrawCandidate> m_DrawCandidates;
        GpuDrawListBuilder m_DrawListBuilder;
        GpuDrawBuffers m_DrawBuffers;
        Vector<uint32_t> m_DirtyInstanceIndices;
        Vector<uint32_t> m_DirtyLightIndices;
        Vector<uint32_t> m_DirtyMeshIndices;
        Vector<DirtyRange> m_InstanceRanges;
        Vector<DirtyRange> m_LightRanges;
        Vector<DirtyRange> m_MeshRanges;
        Vector<DirtyRange> m_MeshLodRanges;
        Vector<DirtyRange> m_MeshletRanges;
        Ref<GenericGpuBuffer> m_InstanceBuffer;
        Ref<GenericGpuBuffer> m_LightBuffer;
        Ref<GenericGpuBuffer> m_ShadowLightBuffer;
        Ref<GenericGpuBuffer> m_ShadowViewBuffer;
        Ref<GenericGpuBuffer> m_MeshBuffer;
        Ref<GenericGpuBuffer> m_MeshLodBuffer;
        Ref<GenericGpuBuffer> m_MeshletBuffer;
        Ref<GenericGpuBuffer> m_MaterialBuffer;
        GpuSceneUploadStats m_Stats;
    };
} // namespace Crowny
