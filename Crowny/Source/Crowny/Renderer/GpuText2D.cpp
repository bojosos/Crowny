#include "cwpch.h"

#include "Crowny/Renderer/GpuText2D.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/RenderAPI/GenericGpuBuffer.h"
#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/RenderAPI/RenderCapabilities.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/MSDFData.h"
#include "Crowny/Renderer/RenderSnapshot.h"
#include "Crowny/Renderer/VisibilityCulling.h"

namespace Crowny
{
    GpuText2D::GpuText2D(uint32_t maxGlyphsPerPage) : m_MaxGlyphsPerPage(std::clamp(maxGlyphsPerPage, 1u, 65536u)) {}

    void GpuText2D::BeginView()
    {
        m_Statistics = {};
        // A sole cache reference means neither extraction nor any queued snapshot
        // can use this layout again. GPU buffer destruction uses backend retirement.
        for (auto it = m_Layouts.begin(); it != m_Layouts.end();)
            if (it->second.Source->GetRefCount() == 1)
                it = m_Layouts.erase(it);
            else
                ++it;
    }

    bool GpuText2D::Build(const Ref<const OwnedTextLayout>& source, Layout& result)
    {
        result.Source = source;
        const Font* primary = source->GetPrimaryFont();
        if (!primary || !primary->IsValid())
            return true;
        const auto layout = source->View();
        if (!layout.LineCount)
            return true;
        const uint64_t limit = RenderAPI::Get().GetCapabilities().MaxStorageBufferRange;
        const uint32_t capacity = static_cast<uint32_t>(std::min<uint64_t>(m_MaxGlyphsPerPage, limit ? limit / sizeof(Glyph) : m_MaxGlyphsPerPage));
        if (!capacity)
            return false;
        m_Scratch.clear();
        m_Scratch.reserve(std::min<size_t>(capacity, std::max(layout.GlyphCount, std::min<size_t>(layout.LineCount, capacity) * 2)));
        Ref<Texture> atlas;
        bool decoration = false;
        auto addBounds = [&](uint32_t kind, const glm::vec4& rect) {
            if (!result.HasBounds[kind])
                result.Bounds[kind] = rect;
            else
                result.Bounds[kind] = { glm::min(glm::vec2(result.Bounds[kind]), glm::vec2(rect)),
                                        glm::max(glm::vec2(result.Bounds[kind].z, result.Bounds[kind].w), glm::vec2(rect.z, rect.w)) };
            result.HasBounds[kind] = true;
        };
        auto flush = [&]() {
            if (m_Scratch.empty())
                return true;
            const uint32_t count = static_cast<uint32_t>(m_Scratch.size());
            auto buffer = GenericGpuBuffer::Create({ count, sizeof(Glyph), GpuBufferType::Structured, BF_UNKNOWN, BufferUsage::BU_STATIC_DRAW });
            if (!buffer)
                return false;
            buffer->WriteData(0, count * sizeof(Glyph), m_Scratch.data());
            result.Pages.push_back({ buffer, atlas, count, decoration });
            m_Statistics.GeometryUploadBytes += count * sizeof(Glyph);
            m_Scratch.clear();
            return true;
        };
        for (size_t i = 0; i < layout.GlyphCount; ++i)
        {
            const TextLayoutGlyph& glyph = layout.Glyphs[i];
            const Font* font = glyph.SourceFont ? glyph.SourceFont : primary;
            const Ref<Texture> texture = font->GetAtlasTexture();
            if (!glyph.Glyph || !texture)
                continue;
            if ((atlas != texture || m_Scratch.size() == capacity) && !flush())
                return false;
            atlas = texture;
            double left, bottom, right, top;
            glyph.Glyph->getQuadPlaneBounds(left, bottom, right, top);
            Glyph record{};
            record.Rect = { glyph.PenPosition.x + float(left * layout.GlyphScale), glyph.PenPosition.y + float(bottom * layout.GlyphScale),
                            glyph.PenPosition.x + float(right * layout.GlyphScale), glyph.PenPosition.y + float(top * layout.GlyphScale) };
            glyph.Glyph->getQuadAtlasBounds(left, bottom, right, top);
            record.UvRect = { float(left) / texture->GetWidth(), float(bottom) / texture->GetHeight(), float(right) / texture->GetWidth(),
                              float(top) / texture->GetHeight() };
            record.Shape = { glyph.PenPosition.y, font->GetAtlasPixelRange(), 0, 0 };
            addBounds(0, record.Rect);
            result.MaxBaselineDistance = std::max(
              result.MaxBaselineDistance, std::max(std::abs(record.Rect.y - glyph.PenPosition.y), std::abs(record.Rect.w - glyph.PenPosition.y)));
            m_Scratch.push_back(record);
        }
        if (!flush())
            return false;
        decoration = true;
        atlas = primary->GetAtlasTexture();
        const auto& metrics = *primary->GetMetrics();
        result.DecorationThickness = std::max(float(std::abs(metrics.underlineThickness) * layout.GlyphScale), layout.FontSize / 36.0f * 0.035f);
        // Both kinds are retained even when disabled. Paint/style changes select
        // them in the shader without rebuilding or uploading glyph placement.
        for (size_t i = 0; i < layout.LineCount; ++i)
        {
            const auto& line = layout.Lines[i];
            if (line.Width <= 0)
                continue;
            for (uint32_t kind = 1; kind <= 2; ++kind)
            {
                if (m_Scratch.size() == capacity && !flush())
                    return false;
                const float center = line.Baseline + float((kind == 1 ? metrics.underlineY : metrics.ascenderY * 0.32) * layout.GlyphScale);
                m_Scratch.push_back({ { line.X, center, line.X + line.Width, center }, {}, { line.Baseline, 0, float(kind), 0 } });
                addBounds(kind, m_Scratch.back().Rect);
            }
        }
        return flush();
    }

    bool GpuText2D::IsVisible(const Layout& layout, const RenderableText& text, const glm::mat4& viewProjection) const
    {
        const auto& paint = text.TextData;
        glm::vec4 bounds{};
        bool hasBounds = false;
        auto include = [&](glm::vec4 rect) {
            if (!hasBounds)
                bounds = rect;
            else
                bounds = { glm::min(glm::vec2(bounds), glm::vec2(rect)), glm::max(glm::vec2(bounds.z, bounds.w), glm::vec2(rect.z, rect.w)) };
            hasBounds = true;
        };
        if (layout.HasBounds[0])
        {
            glm::vec4 glyphBounds = layout.Bounds[0];
            const float slant = paint.FontStyle.IsSet(TextFontStyleBits::Italic) ? 0.2f * layout.MaxBaselineDistance : 0;
            glyphBounds.x -= slant;
            glyphBounds.z += slant;
            include(glyphBounds);
            if (paint.ShadowColor.a > 0)
                include(glyphBounds + glm::vec4(paint.ShadowOffset, paint.ShadowOffset));
        }
        for (uint32_t kind = 1; kind <= 2; ++kind)
        {
            if (!layout.HasBounds[kind] || !paint.FontStyle.IsSet(kind == 1 ? TextFontStyleBits::Underline : TextFontStyleBits::Strikethrough))
                continue;
            glm::vec4 decoration = layout.Bounds[kind];
            const float halfThickness = 0.5f * (paint.DecorationThickness > 0 ? paint.DecorationThickness : layout.DecorationThickness);
            const float offset = kind == 1 ? paint.UnderlineOffset : paint.StrikethroughOffset;
            decoration.y += offset - halfThickness;
            decoration.w += offset + halfThickness;
            include(decoration);
        }
        if (!hasBounds)
            return false;
        if (paint.ClipToBounds)
        {
            if (paint.LayoutSize.x > 0)
            {
                bounds.x = std::max(bounds.x, 0.0f);
                bounds.z = std::min(bounds.z, paint.LayoutSize.x);
            }
            if (paint.LayoutSize.y > 0)
            {
                bounds.y = std::max(bounds.y, -paint.LayoutSize.y);
                bounds.w = std::min(bounds.w, 0.0f);
            }
        }
        if (bounds.x >= bounds.z || bounds.y >= bounds.w)
            return false;
        const glm::vec2 halfSize = 0.5f * (glm::vec2(bounds.z, bounds.w) - glm::vec2(bounds));
        const glm::vec3 center = text.WorldMatrix * glm::vec4(glm::vec2(bounds) + halfSize, 0, 1);
        // Sum of transformed half-axes remains conservative under shear.
        const float radius = glm::length(glm::vec3(text.WorldMatrix[0])) * halfSize.x + glm::length(glm::vec3(text.WorldMatrix[1])) * halfSize.y;
        return VisibilityFrustum::FromViewProjection(viewProjection).IntersectsSphere(center, radius);
    }

    bool GpuText2D::Prepare(const Ref<const OwnedTextLayout>& layout)
    {
        if (!layout || m_Layouts.contains(layout.get()))
            return true;
        Layout built;
        if (!Build(layout, built))
            return false;
        if (!built.Pages.empty() && !m_Material.IsValid())
        {
            const auto shader = AssetManager::Get().Load<Shader>("Resources/Shaders/Text2D.asset");
            const auto depth = CreateRef<DepthStencilStateDesc>();
            depth->EnableDepthWrite = false;
            depth->DepthCompareFunction = RenderAPI::Get().GetCapabilities().GetFeatureTier() == RenderFeatureTier::Compatibility
                                            ? CompareFunction::LESS_EQUAL
                                            : CompareFunction::GREATER_EQUAL;
            if (!m_Material.Initialize(shader, nullptr, depth))
                return false;
            m_EmptyLayout = CreateRef<BufferLayout>();
        }
        m_Layouts.emplace(layout.get(), std::move(built));
        ++m_Statistics.GeometryBuilds;
        return true;
    }

    bool GpuText2D::Render(const RenderableText& text, const glm::mat4& viewProjection)
    {
        if (!text.Layout || text.TextData.Text.empty())
            return true;
        const auto found = m_Layouts.find(text.Layout.get());
        if (found == m_Layouts.end())
            return false;
        const Layout& layout = found->second;
        if (layout.Pages.empty())
            return true;
        if (!IsVisible(layout, text, viewProjection))
        {
            ++m_Statistics.CulledObjects;
            return true;
        }
        struct alignas(16) Constants
        {
            glm::mat4 ViewProjection;
            glm::mat4 World;
            glm::vec4 Color;
            glm::vec4 OutlineColor;
            glm::vec4 ClipRect;
            glm::vec4 Style;      // italic, weight, outline, softness
            glm::vec4 Decoration; // thickness, underline offset, strike offset, enabled bits
            glm::vec4 Offset;
            glm::ivec4 Metadata; // object ID, clip enabled, decoration pass, reserved
        } constants{};
        static_assert(sizeof(Constants) == 240);
        const auto& paint = text.TextData;
        const bool clipX = paint.ClipToBounds && paint.LayoutSize.x > 0;
        const bool clipY = paint.ClipToBounds && paint.LayoutSize.y > 0;
        const float extent = std::numeric_limits<float>::max() * 0.25f;
        constants.ViewProjection = viewProjection;
        constants.World = text.WorldMatrix;
        constants.ClipRect = { clipX ? 0 : -extent, clipY ? -paint.LayoutSize.y : -extent, clipX ? paint.LayoutSize.x : extent, clipY ? 0 : extent };
        constants.Style = { paint.FontStyle.IsSet(TextFontStyleBits::Italic) ? 0.2f : 0, paint.FontStyle.IsSet(TextFontStyleBits::Bold) ? 0.075f : 0,
                            std::max(0.0f, paint.Thickness), 0 };
        const int decorationBits =
          (paint.FontStyle.IsSet(TextFontStyleBits::Underline) ? 1 : 0) | (paint.FontStyle.IsSet(TextFontStyleBits::Strikethrough) ? 2 : 0);
        constants.Decoration = { paint.DecorationThickness > 0 ? paint.DecorationThickness : layout.DecorationThickness, paint.UnderlineOffset,
                                 paint.StrikethroughOffset, float(decorationBits) };
        constants.Metadata = { text.EntityId, clipX || clipY, 0, 0 };
        auto draw = [&](bool decorations) {
            if (!m_Material.WriteUniformBlock(0, 0, &constants, sizeof(constants)))
                return false;
            m_Statistics.UniformUploadBytes += sizeof(constants);
            for (const Page& page : layout.Pages)
            {
                if (page.Decoration != decorations)
                    continue;
                if (!m_Material.SetBuffer(0, 2, page.Buffer) || !m_Material.SetTexture(0, 1, page.Atlas) || !m_Material.Bind())
                    return false;
                RenderAPI::Get().SetVertexLayout(m_EmptyLayout);
                RenderAPI::Get().SetDrawMode(DrawMode::TRIANGLE_LIST);
                RenderAPI::Get().Draw(0, 6, page.Count);
                ++m_Statistics.Batches;
            }
            return true;
        };
        if (paint.ShadowColor.a > 0)
        {
            constants.Color = constants.OutlineColor = paint.ShadowColor;
            constants.Offset = { paint.ShadowOffset, 0, 0 };
            constants.Style.z = 0;
            constants.Style.w = std::max(0.0f, paint.ShadowSoftness);
            if (!draw(false))
                return false;
        }
        constants.Offset = {};
        constants.Style.w = 0;
        if (decorationBits)
        {
            constants.Color = paint.UseCustomDecorationColor ? paint.DecorationColor : paint.Color;
            constants.Metadata.z = 1;
            if (!draw(true))
                return false;
        }
        constants.Metadata.z = 0;
        constants.Color = paint.Color;
        constants.OutlineColor = paint.OutlineColor;
        constants.Style.z = std::max(0.0f, paint.Thickness);
        return draw(false);
    }
} // namespace Crowny
