#pragma once

#include "Crowny/Ecs/Components.h"
#include "Crowny/Renderer/TextLayout.h"

namespace Crowny
{
    // Immutable layout storage pins the concrete font objects containing the
    // glyph geometry. Reimporting an asset or removing a fallback cannot invalidate
    // an already submitted snapshot.
    class OwnedTextLayout final : public RefCounted
    {
    public:
        OwnedTextLayout(const TextLayoutResult& result, std::span<const Ref<Font>> fonts);
        TextLayoutResult View() const;
        const Font* GetPrimaryFont() const { return m_Fonts.empty() ? nullptr : m_Fonts.front().get(); }

    private:
        TextLayoutResult m_Metrics;
        Vector<TextLayoutGlyph> m_Glyphs;
        Vector<TextLayoutLine> m_Lines;
        Vector<TextLayoutCaret> m_Carets;
        Vector<TextLayoutFontRun> m_FontRuns;
        Vector<Ref<Font>> m_Fonts;
    };

    class TextLayoutCache final
    {
    public:
        void BeginExtraction();
        Ref<const OwnedTextLayout> Get(uint64_t identity, const TextComponent& text);
        void EndExtraction();
        void Clear();
        uint64_t GetBuildCount() const { return m_BuildCount; }
        static bool SameLayout(const TextComponent& a, const TextComponent& b);

    private:
        struct Entry
        {
            TextComponent Key;
            Vector<Ref<Font>> Fonts;
            Ref<const OwnedTextLayout> Layout;
            bool Seen = false;
        };
        UnorderedMap<uint64_t, Entry> m_Entries;
        Vector<Ref<Font>> m_FontScratch;
        Vector<Ref<Font>> m_FontStack;
        TextLayoutScratch m_Scratch;
        uint64_t m_BuildCount = 0;
    };
} // namespace Crowny
