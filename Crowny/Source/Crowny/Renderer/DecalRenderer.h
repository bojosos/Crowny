#pragma once

#include "Crowny/RenderAPI/GenericGpuBuffer.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/Decal.h"
#include "Crowny/Renderer/DecalAtlas.h"
#include "Crowny/Renderer/DecalGpuGrid.h"
#include "Crowny/Renderer/DecalWorld.h"

namespace Crowny
{
    class Scene;
    class Material;
    class GraphicsMaterial;
    class GpuScene;
    class GpuDecalWorld;
    struct RenderSnapshot;

    struct alignas(16) GpuDecalData
    {
        glm::mat4 WorldToLocal{ 1.0f };
        glm::vec4 SizeProjection{ 1, 1, 0.2f, 0 };
        glm::vec4 Cylinder{ 0.5f, 0.5f, 1, 0.1f };
        glm::vec4 OffsetArc{ 0, 0, 0, 6.2831853f };
        glm::vec4 Fades{ 0, 0, 0.5f, 0.0871557f };
        glm::vec4 Tint{ 1 };
        glm::vec4 UV{ 1, 1, 0, 0 };
        glm::vec4 Controls{ 0 };  // UV rotation, seam rotation, distance start/end
        glm::uvec4 Metadata{ 0 }; // layers, target DFS begin/end, material index
    };
    static_assert(sizeof(GpuDecalData) == 192);

    struct alignas(16) GpuDecalMaterial
    {
        glm::vec4 Color{ 1 };
        glm::vec4 Emission{ 0 };
        glm::vec4 Surface{ 0.5f, 0, 1, 1 };
        glm::vec4 Strengths{ 1 };
        glm::vec4 Strengths2{ 1 };
        glm::vec4 CorrectionTintExposure{ 1, 1, 1, 0 };
        glm::vec4 Correction{ 0, 1, 1, 0 }; // hue radians, saturation, contrast, mask threshold
        glm::vec4 Mask{ 0 };                // softness
        glm::uvec4 Modes{ 1, 0, 0, 2 };     // channel mask, color/roughness/emission blend
        glm::uvec4 Textures{ 0 };
        glm::uvec4 Textures2{ 0 }; // coverage texture, replace normal
    };
    static_assert(sizeof(GpuDecalMaterial) == 176);

    struct alignas(16) GpuDecalSlot
    {
        GpuDecalData Data;
        glm::uvec4 Generations{ 0 }; // decal generation, material generation
    };
    static_assert(sizeof(GpuDecalSlot) == 208);

    struct alignas(16) GpuDecalMaterialSlot
    {
        GpuDecalMaterial Data;
        glm::uvec4 Generation{ 0 };
    };
    static_assert(sizeof(GpuDecalMaterialSlot) == 192);

    struct RenderableDecal
    {
        UUID Id;
        UUID MaterialId;
        DecalHandle Handle;
        int32_t SortOrder = 0;
        uint64_t MaterialRevision = 0;
        GpuDecalData Data;
        GpuDecalMaterial Material;
        glm::vec4 Bounds{ 0 };
        std::array<Ref<Texture>, 5> Textures;
    };

    struct RenderableDecalChange
    {
        DecalHandle Handle;
        DecalChangeType Type = DecalChangeType::Create;
        RenderableDecal Record;
    };

    // Simulation-side extraction state. Snapshots own the changed material/texture
    // values and the lifetime token; no render thread reads live components.
    class DecalExtractionState
    {
    public:
        void Clear();

    private:
        friend class DecalRenderer;
        void BeginSnapshot(RenderSnapshot& snapshot);
        void EndSnapshot(RenderSnapshot& snapshot);
        struct Tracked
        {
            RenderableDecal Source;
            bool Seen = false;
        };
        DecalWorld m_World;
        UnorderedMap<UUID, Tracked> m_Tracked;
        Vector<DecalChange> m_Changes;
        std::shared_ptr<const uint8_t> m_Lifetime = std::make_shared<uint8_t>(0);
    };

    struct DecalRenderStats
    {
        uint32_t Visible = 0;
        uint32_t TextureCount = 0;
        uint32_t RejectedMaterials = 0;
        uint32_t OverflowClusters = 0;
        uint32_t MaxClusterCandidates = 0;
        uint32_t OccupiedClusters = 0;
        uint64_t StatisticsFrame = 0;
        bool ClusterStatisticsAvailable = false;
        float GridGpuMilliseconds = 0;
        uint64_t UploadedBytes = 0;
        bool GpuBuiltLists = false;
        uint32_t ListValidationFailures = 0;
    };

    class DecalRenderer
    {
    public:
        static void Extract(Scene& scene, RenderSnapshot& snapshot, DecalExtractionState* state = nullptr);
        void Prepare(const RenderSnapshot& snapshot, GpuScene* gpuScene = nullptr, GpuDecalWorld* world = nullptr);
        void Bind(GraphicsMaterial& material);
        void BindCoatingMode(GraphicsMaterial& material, uint32_t mode) const;
        bool HasCoatings() const;
        void PrepareCompatibility(const RenderSnapshot& snapshot, GpuDecalWorld* world = nullptr);
        void BindCompatibility(Material& material, uint32_t objectId, uint32_t coatingMode = 0);
        void ReleaseView(uint64_t view);
        const DecalRenderStats& GetStats() const { return m_Stats; }
        bool HistoryChanged() const { return m_HistoryChanged; }
        const glm::vec4& GetHistoryInvalidationRect() const { return m_HistoryInvalidation; }

    private:
        template <typename T> void Upload(Ref<GenericGpuBuffer>& buffer, Vector<T>& previous, const Vector<T>& data);
        Ref<GenericGpuBuffer> m_Decals, m_Materials, m_Receivers;
        Ref<GenericGpuBuffer> m_FallbackDecals, m_FallbackMaterials, m_Visible;
        Vector<GpuDecalSlot> m_PreviousDecals;
        Vector<GpuDecalMaterialSlot> m_PreviousMaterials;
        Vector<glm::uvec2> m_PreviousVisible;
        Vector<glm::uvec4> m_PreviousReceivers;
        Vector<Ref<Texture>> m_Textures;
        DecalRenderStats m_Stats;
        glm::uvec4 m_Counts{ 0 };
        DecalAtlas m_Atlas;
        struct alignas(16) GridConstants
        {
            glm::mat4 ViewProjection{ 1 };
            glm::vec4 DepthRow{ 0, 0, -1, 0 };
            glm::uvec4 Dimensions{ 1, 1, 24, 16 };
            glm::vec4 DepthViewport{ 0.05f, 1000, 1, 1 };
            glm::vec4 CameraPosition{ 0 };
        } m_GridConstants;
        DecalClusterGrid m_Grid;
        DecalGpuGrid m_GpuGrid;
        Ref<GenericGpuBuffer> m_Cells, m_Indices;
        Vector<glm::uvec2> m_PreviousCells;
        Vector<uint32_t> m_PreviousIndices;
        struct ViewHistory
        {
            uint64_t Revision = 0;
            uint32_t DebugView = 0;
            Vector<glm::vec4> Bounds;
        };
        UnorderedMap<uint64_t, ViewHistory> m_ViewRevisions;
        glm::vec4 m_HistoryInvalidation{ 1, 1, 0, 0 };
        bool m_HistoryChanged = false;
        bool m_HasCoatings = false;
    };
} // namespace Crowny
