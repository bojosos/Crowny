#include "cwpch.h"

#include "Crowny/Renderer/FontManager.h"
#include "Crowny/Renderer/TextLayoutCache.h"

namespace Crowny
{
    OwnedTextLayout::OwnedTextLayout(const TextLayoutResult& result, std::span<const Ref<Font>> fonts)
      : m_Metrics(result), m_Fonts(fonts.begin(), fonts.end())
    {
        if (result.GlyphCount)
            m_Glyphs.assign(result.Glyphs, result.Glyphs + result.GlyphCount);
        if (result.LineCount)
            m_Lines.assign(result.Lines, result.Lines + result.LineCount);
        if (result.CaretCount)
            m_Carets.assign(result.Carets, result.Carets + result.CaretCount);
        if (result.FontRunCount)
            m_FontRuns.assign(result.FontRuns, result.FontRuns + result.FontRunCount);
        m_Metrics.Glyphs = nullptr;
        m_Metrics.Lines = nullptr;
        m_Metrics.Carets = nullptr;
        m_Metrics.FontRuns = nullptr;
    }

    TextLayoutResult OwnedTextLayout::View() const
    {
        TextLayoutResult result = m_Metrics;
        result.Glyphs = m_Glyphs.data();
        result.Lines = m_Lines.data();
        result.Carets = m_Carets.data();
        result.FontRuns = m_FontRuns.data();
        return result;
    }

    bool TextLayoutCache::SameLayout(const TextComponent& a, const TextComponent& b)
    {
        return a.Text == b.Text && a.Size == b.Size && a.AutoSize == b.AutoSize && a.AutoSizeMin == b.AutoSizeMin && a.AutoSizeMax == b.AutoSizeMax &&
               a.LayoutSize == b.LayoutSize && a.Wrapping == b.Wrapping && a.WrapMode == b.WrapMode && a.Overflow == b.Overflow &&
               a.MaxLines == b.MaxLines && a.HorizontalAlignment == b.HorizontalAlignment && a.VerticalAlignment == b.VerticalAlignment &&
               a.CharacterSpacing == b.CharacterSpacing && a.WordSpacing == b.WordSpacing && a.LineSpacing == b.LineSpacing &&
               a.ParagraphSpacing == b.ParagraphSpacing && a.TabWidth == b.TabWidth && a.UseKerning == b.UseKerning;
    }

    void TextLayoutCache::BeginExtraction()
    {
        for (auto& [_, entry] : m_Entries)
            entry.Seen = false;
    }

    Ref<const OwnedTextLayout> TextLayoutCache::Get(uint64_t identity, const TextComponent& text)
    {
        const AssetHandle<Font> font = text.Font ? text.Font : FontManager::GetDefaultFont();
        if (!font || !font->IsValid())
            return nullptr;
        m_FontScratch.clear();
        m_FontStack.clear();
        m_FontStack.push_back(font.GetInternalPtr());
        while (!m_FontStack.empty())
        {
            const Ref<Font> current = m_FontStack.back();
            m_FontStack.pop_back();
            if (std::find(m_FontScratch.begin(), m_FontScratch.end(), current) != m_FontScratch.end())
                continue;
            m_FontScratch.push_back(current);
            const auto& fallbacks = current->GetFallbackFonts();
            for (auto child = fallbacks.rbegin(); child != fallbacks.rend(); ++child)
                if (*child)
                    m_FontStack.push_back(child->GetInternalPtr());
        }
        Entry& entry = m_Entries[identity];
        entry.Seen = true;
        if (!entry.Layout || !SameLayout(entry.Key, text) || entry.Fonts != m_FontScratch)
        {
            const TextLayoutResult result = TextLayout::Build(text, *font, m_Scratch);
            entry.Layout = CreateRef<OwnedTextLayout>(result, std::span<const Ref<Font>>(m_FontScratch));
            entry.Key = text;
            entry.Fonts = m_FontScratch;
            ++m_BuildCount;
        }
        return entry.Layout;
    }

    void TextLayoutCache::EndExtraction()
    {
        for (auto entry = m_Entries.begin(); entry != m_Entries.end();)
        {
            if (entry->second.Seen)
                ++entry;
            else
                entry = m_Entries.erase(entry);
        }
        m_FontScratch.clear();
        m_FontStack.clear();
    }

    void TextLayoutCache::Clear()
    {
        m_Entries.clear();
        m_FontScratch.clear();
        m_FontStack.clear();
    }
} // namespace Crowny
