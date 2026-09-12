#pragma once

#include "Crowny/Renderer/ComputeMaterial.h"
#include "Crowny/Renderer/TextLayoutCache.h"

namespace Crowny
{
    struct RenderableText;

    struct GpuText2DStatistics
    {
        uint64_t GeometryUploadBytes = 0;
        uint64_t UniformUploadBytes = 0;
        uint32_t GeometryBuilds = 0;
        uint32_t Batches = 0;
        uint32_t CulledObjects = 0;
    };

    // Render-thread cache of immutable local-space geometry. Cameras and text
    // paint share these pages; only the small draw constants depend on the view.
    class GpuText2D final
    {
    public:
        explicit GpuText2D(uint32_t maxGlyphsPerPage = 65536);
        void BeginView();
        bool Prepare(const Ref<const OwnedTextLayout>& layout);
        bool Render(const RenderableText& text, const glm::mat4& viewProjection);
        const GpuText2DStatistics& GetStatistics() const { return m_Statistics; }
        size_t GetCachedLayoutCount() const { return m_Layouts.size(); }

    private:
        struct alignas(16) Glyph
        {
            glm::vec4 Rect;
            glm::vec4 UvRect;
            // Baseline, MSDF range, decoration kind (0=glyph, 1=underline, 2=strike), reserved.
            glm::vec4 Shape;
        };
        static_assert(sizeof(Glyph) == 48);
        struct Page
        {
            Ref<GenericGpuBuffer> Buffer;
            Ref<Texture> Atlas;
            uint32_t Count = 0;
            bool Decoration = false;
        };
        struct Layout
        {
            Ref<const OwnedTextLayout> Source;
            Vector<Page> Pages;
            float DecorationThickness = 0;
            float MaxBaselineDistance = 0;
            // Min/max local bounds for glyphs, underlines, and strikethroughs.
            Array<glm::vec4, 3> Bounds{};
            Array<bool, 3> HasBounds{};
        };
        bool Build(const Ref<const OwnedTextLayout>& source, Layout& result);
        bool IsVisible(const Layout& layout, const RenderableText& text, const glm::mat4& viewProjection) const;

        uint32_t m_MaxGlyphsPerPage;
        Vector<Glyph> m_Scratch;
        UnorderedMap<const OwnedTextLayout*, Layout> m_Layouts;
        GraphicsMaterial m_Material;
        Ref<BufferLayout> m_EmptyLayout;
        GpuText2DStatistics m_Statistics;
    };
} // namespace Crowny
