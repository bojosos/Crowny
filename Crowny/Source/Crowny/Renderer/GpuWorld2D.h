#pragma once

#include "Crowny/Renderer/ComputeMaterial.h"
#include "Crowny/Renderer/DrawList2D.h"
#include "Crowny/Renderer/GpuText2D.h"
#include "Crowny/Renderer/RenderWorld2D.h"

namespace Crowny
{
    struct RenderSnapshot;
    struct RenderableSprite;

    struct GpuWorld2DStatistics
    {
        uint64_t UploadedBytes = 0;
        uint64_t InstanceUploadBytes = 0;
        uint64_t OrderUploadBytes = 0;
        uint64_t UniformUploadBytes = 0;
        uint64_t GlyphUploadBytes = 0;
        uint32_t TextGeometryBuilds = 0;
        uint32_t CulledTextObjects = 0;
        uint32_t Submitted = 0;
        uint32_t Visible = 0;
        uint32_t Batches = 0;
        uint32_t Glyphs = 0;
        uint32_t OrderCacheMisses = 0;
        uint32_t DrawListCacheHits = 0;
        bool GpuCulling = false;
        // Visible belongs to this completed frame on GPU, or the current frame
        // on CPU. An unavailable GPU sample is distinct from zero visibility.
        bool VisibilitySampleValid = true;
        uint64_t VisibilityFrameNumber = 0;
        double UploadCpuTimeMs = 0.0;
        double PrepareCpuTimeMs = 0.0;
        double SubmissionCpuTimeMs = 0.0;
        Array<uint32_t, static_cast<size_t>(BatchBreak2D::Count)> BatchBreaks{};
    };

    // Render-thread mirror. Owns persistent GPU content independently of camera
    // draw order. Its caller owns the lifetime and supplies snapshots in order.
    class GpuWorld2D final
    {
    public:
        // The device storage-buffer limit may reduce these page sizes further.
        explicit GpuWorld2D(uint32_t maxInstancesPerPage = 65536, uint32_t maxOrderEntriesPerPage = 65536, bool enableGpuCulling = true);
        void Apply(std::span<const RenderChange2D> changes);
        bool GetSprite(RenderHandle2D handle, RenderableSprite& output) const;
        void ReleaseView(uint64_t viewIdentity);
        // GPU dispatch resumes snapshot.Target with attachment loads. Producers
        // without a target use CPU culling on the caller's bound framebuffer.
        bool Render(const RenderSnapshot& snapshot);
        const GpuWorld2DStatistics& GetStatistics() const { return m_Statistics; }

    private:
        struct alignas(16) Instance
        {
            RenderInstance2D Data;
            glm::uvec4 Metadata{ 0 };
        };
        static_assert(sizeof(Instance) == 144);
        struct Slot
        {
            RenderHandle2D Handle;
            uint32_t Texture = 0;
            bool Visible = false;
        };
        struct Page
        {
            Ref<GenericGpuBuffer> Buffer;
        };
        struct TextureEntry
        {
            Ref<Texture> Resource;
            uint32_t Users = 0;
        };
        struct OrderPage
        {
            Ref<GenericGpuBuffer> Buffer;
            Vector<DrawInstance2D> PreviousOrder;
        };
        struct View
        {
            Vector<OrderPage> OrderPages;
            DrawList2D DrawList;
            Vector<RenderHandle2D> CandidateHandles;
            uint64_t PreparedRevision = 0;
            uint32_t PreparedSubmitted = 0;
            bool Prepared = false;
            Ref<GenericGpuBuffer> Prefix, GroupCounts, CompactedOrder, Arguments, VisibleCount;
            struct PendingCount
            {
                Ref<GpuBufferReadback> Readback;
                uint64_t FrameNumber = 0;
            };
            Array<PendingCount, 4> PendingCounts{};
            uint64_t VisibilityFrameNumber = 0;
            uint32_t Visible = 0;
            bool VisibilitySampleValid = false;
        };
        uint32_t AcquireTexture(const Ref<Texture>& texture);
        void ReleaseTexture(uint32_t index);
        bool Upload();
        bool Prepare(const RenderSnapshot& snapshot, View& view);
        bool Compact(const RenderSnapshot& snapshot, View& view, const Ref<GenericGpuBuffer>& instances, const Ref<GenericGpuBuffer>& order,
                     uint32_t first, uint32_t count, bool firstDraw);

        Vector<Instance> m_Instances;
        Vector<Slot> m_Slots;
        Vector<uint32_t> m_Dirty;
        uint64_t m_PreparationRevision = 0;
        Vector<TextureEntry> m_Textures{ 1 };
        Vector<uint32_t> m_FreeTextures;
        UnorderedMap<const Texture*, uint32_t> m_TextureLookup;
        Vector<Page> m_Pages;
        uint32_t m_MaxInstancesPerPage;
        uint32_t m_InstancesPerPage = 0;
        uint32_t m_MaxOrderEntriesPerPage;
        uint32_t m_OrderEntriesPerPage = 0;
        GraphicsMaterial m_Material;
        ComputeMaterial m_Compactor;
        Ref<IndexBuffer> m_QuadIndices;
        bool m_EnableGpuCulling;
        GpuText2D m_Text;
        Ref<BufferLayout> m_EmptyLayout;
        UnorderedMap<uint64_t, View> m_Views;
        GpuWorld2DStatistics m_Statistics;
    };
} // namespace Crowny
