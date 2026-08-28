#include "cwpch.h"

#include "Crowny/Renderer/TextLayout.h"

#include "Crowny/Common/UTF8.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Renderer/MSDFdata.h"

#include <limits>

namespace Crowny
{
    namespace
    {
        constexpr size_t INVALID_TEXT_INDEX = std::numeric_limits<size_t>::max();

        bool IsTextWhitespace(char32_t codePoint)
        {
            return codePoint == U'\t' || codePoint == U' ' || codePoint == 0x00A0 || codePoint == 0x1680 ||
                   (codePoint >= 0x2000 && codePoint <= 0x200A) || codePoint == 0x202F || codePoint == 0x205F || codePoint == 0x3000;
        }

        bool IsCombiningMark(char32_t codePoint)
        {
            return (codePoint >= 0x0300 && codePoint <= 0x036F) || (codePoint >= 0x1AB0 && codePoint <= 0x1AFF) ||
                   (codePoint >= 0x1DC0 && codePoint <= 0x1DFF) || (codePoint >= 0x20D0 && codePoint <= 0x20FF) ||
                   (codePoint >= 0xFE20 && codePoint <= 0xFE2F);
        }

        bool IsBreakableWhitespace(char32_t codePoint) { return IsTextWhitespace(codePoint) && codePoint != 0x00A0 && codePoint != 0x202F; }

        bool IsCJKCharacter(char32_t codePoint)
        {
            return (codePoint >= 0x2E80 && codePoint <= 0xA4CF) || (codePoint >= 0xAC00 && codePoint <= 0xD7AF) ||
                   (codePoint >= 0xF900 && codePoint <= 0xFAFF) || (codePoint >= 0x20000 && codePoint <= 0x323AF);
        }

        bool IsClosingPunctuation(char32_t codePoint)
        {
            switch (codePoint)
            {
            case U')':
            case U']':
            case U'}':
            case U',':
            case U'.':
            case U'!':
            case U'?':
            case U':':
            case U';':
            case 0x3001:
            case 0x3002:
            case 0xFF01:
            case 0xFF09:
            case 0xFF0C:
            case 0xFF0E:
            case 0xFF1A:
            case 0xFF1B:
            case 0xFF1F:
                return true;
            default:
                return false;
            }
        }

        bool IsExplicitBreakCharacter(char32_t codePoint)
        {
            return codePoint == U'-' || codePoint == U'/' || codePoint == 0x00AD || codePoint == 0x200B;
        }

        bool IsVariationSelector(char32_t codePoint)
        {
            return (codePoint >= 0xFE00 && codePoint <= 0xFE0F) || (codePoint >= 0xE0100 && codePoint <= 0xE01EF);
        }

        bool IsEmojiModifier(char32_t codePoint) { return codePoint >= 0x1F3FB && codePoint <= 0x1F3FF; }

        bool IsRegionalIndicator(char32_t codePoint) { return codePoint >= 0x1F1E6 && codePoint <= 0x1F1FF; }

        bool ContinuesCluster(const FrameVector<TextLayoutToken>& tokens, size_t index, uint32_t regionalIndicatorCount)
        {
            if (index == 0 || tokens[index].NewLine || tokens[index - 1].NewLine)
                return false;

            const char32_t codePoint = tokens[index].CodePoint;
            const char32_t previous = tokens[index - 1].CodePoint;
            if (IsCombiningMark(codePoint) || IsVariationSelector(codePoint) || IsEmojiModifier(codePoint) || codePoint == 0x200D ||
                previous == 0x200D)
                return true;

            return IsRegionalIndicator(previous) && IsRegionalIndicator(codePoint) && regionalIndicatorCount % 2 == 1;
        }

        void AssignClusterRanges(TextLayoutScratch& scratch)
        {
            if (scratch.Tokens.Empty())
                return;

            size_t clusterStart = 0;
            uint32_t regionalIndicatorCount = IsRegionalIndicator(scratch.Tokens[0].CodePoint) ? 1U : 0U;
            for (size_t index = 1; index <= scratch.Tokens.Size(); index++)
            {
                const bool atEnd = index == scratch.Tokens.Size();
                const bool continues = !atEnd && ContinuesCluster(scratch.Tokens, index, regionalIndicatorCount);
                if (!continues)
                {
                    const size_t clusterByteStart = scratch.Tokens[clusterStart].SourceByteStart;
                    const size_t clusterByteEnd = scratch.Tokens[index - 1].SourceByteEnd;
                    for (size_t tokenIndex = clusterStart; tokenIndex < index; tokenIndex++)
                    {
                        scratch.Tokens[tokenIndex].ClusterByteStart = clusterByteStart;
                        scratch.Tokens[tokenIndex].ClusterByteEnd = clusterByteEnd;
                    }
                    clusterStart = index;
                    regionalIndicatorCount = 0;
                }

                if (!atEnd)
                {
                    if (IsRegionalIndicator(scratch.Tokens[index].CodePoint))
                        regionalIndicatorCount++;
                    else if (!continues)
                        regionalIndicatorCount = 0;
                }
            }
        }

        struct ResolvedGlyph
        {
            const Font* SourceFont = nullptr;
            const msdf_atlas::GlyphGeometry* Glyph = nullptr;
            char32_t CodePoint = 0;
        };

        template <typename FontType> ResolvedGlyph ResolveGlyph(const FontType& font, char32_t codePoint)
        {
            if constexpr (requires { font.ResolveGlyph(codePoint); })
            {
                const auto lookup = font.ResolveGlyph(codePoint);
                return { lookup.SourceFont, lookup.Glyph, lookup.ResolvedCodePoint };
            }
            else
            {
                char32_t resolvedCodePoint = 0;
                const msdf_atlas::GlyphGeometry* glyph = font.GetGlyph(codePoint, &resolvedCodePoint);
                return { glyph != nullptr ? &font : nullptr, glyph, resolvedCodePoint };
            }
        }

        template <typename ComponentType> uint32_t ResolveTabWidth(const ComponentType& component, const Font& font)
        {
            if constexpr (requires { component.TabWidth; })
                return std::max(1U, static_cast<uint32_t>(component.TabWidth));
            else
                return font.GetTabWidth();
        }

        void ResolveTokenFonts(const TextComponent& component, const Font& font, TextLayoutScratch& scratch)
        {
            for (size_t index = 0; index < scratch.Tokens.Size(); index++)
            {
                TextLayoutToken& token = scratch.Tokens[index];
                if (!token.NewLine && !token.Invisible && token.CodePoint != U'\t')
                {
                    const ResolvedGlyph resolved = ResolveGlyph(font, token.CodePoint);
                    token.ResolvedCodePoint = resolved.CodePoint;
                    token.Glyph = resolved.Glyph;
                    token.SourceFont = resolved.SourceFont;
                    token.Renderable = !token.WhiteSpace && resolved.Glyph != nullptr;
                }
            }

            for (size_t index = 0; index < scratch.Tokens.Size(); index++)
            {
                TextLayoutToken& token = scratch.Tokens[index];
                if (token.NewLine || token.Invisible || token.CodePoint == U'\t' || token.SourceFont == nullptr)
                    continue;

                char32_t nextCodePoint = 0;
                if (index + 1 < scratch.Tokens.Size() && token.SourceFont == scratch.Tokens[index + 1].SourceFont &&
                    !scratch.Tokens[index + 1].NewLine)
                    nextCodePoint = scratch.Tokens[index + 1].ResolvedCodePoint;
                token.Advance = token.SourceFont->GetAdvance(token.ResolvedCodePoint, nextCodePoint, component.UseKerning);
            }
        }

        bool TokensShareCluster(const TextLayoutToken& left, const TextLayoutToken& right)
        {
            return left.ClusterByteEnd > left.ClusterByteStart && left.ClusterByteStart == right.ClusterByteStart &&
                   left.ClusterByteEnd == right.ClusterByteEnd;
        }

        double TokenAdvance(const TextLayoutToken& token, double penX, double glyphScale, const TextComponent& component,
                            const TextLayoutFontData& fontData)
        {
            if (token.NewLine || token.CodePoint == 0x200B || token.CodePoint == 0x00AD || token.CodePoint == 0x200D ||
                IsVariationSelector(token.CodePoint))
                return 0.0;

            if (token.CodePoint == U'\t')
            {
                const double space = std::max(0.000001, fontData.SpaceAdvance * glyphScale + component.CharacterSpacing + component.WordSpacing);
                const double tabStop = space * static_cast<double>(std::max(1U, fontData.TabWidth));
                const double remainder = std::fmod(std::max(0.0, penX), tabStop);
                return remainder <= 0.000001 ? tabStop : tabStop - remainder;
            }

            double advance = token.Advance * glyphScale + component.CharacterSpacing;
            if (token.WhiteSpace)
                advance += component.WordSpacing;
            return std::max(0.0, advance);
        }

        size_t TrimTrailingWhitespace(const FrameVector<TextLayoutToken>& tokens, size_t begin, size_t end)
        {
            while (end > begin && (tokens[end - 1].WhiteSpace || tokens[end - 1].Invisible))
                end--;
            return end;
        }

        double MeasureTokenRange(const FrameVector<TextLayoutToken>& tokens, size_t begin, size_t end, double glyphScale,
                                 const TextComponent& component, const TextLayoutFontData& fontData)
        {
            end = TrimTrailingWhitespace(tokens, begin, end);
            double pen = 0.0;
            double width = 0.0;
            for (size_t index = begin; index < end; index++)
            {
                const TextLayoutToken& token = tokens[index];
                const double advance = TokenAdvance(token, pen, glyphScale, component, fontData);
                pen += advance;
                if (!token.WhiteSpace && !token.Invisible)
                    width = std::max(0.0, pen - static_cast<double>(component.CharacterSpacing));
            }
            return width;
        }

        void AddLayoutLine(TextLayoutScratch& scratch, size_t begin, size_t end, bool paragraphEnd)
        {
            TextLayoutLine& line = scratch.Lines.Acquire();
            line = {};
            line.TokenStart = begin;
            line.TokenEnd = end;
            line.RenderTokenEnd = end;
            line.ParagraphEnd = paragraphEnd;
        }

        size_t AdvanceToClusterEnd(const TextLayoutScratch& scratch, size_t index)
        {
            while (index < scratch.Tokens.Size() && index > 0 && TokensShareCluster(scratch.Tokens[index - 1], scratch.Tokens[index]))
                index++;
            return index;
        }

        void BuildLineRanges(const TextComponent& component, const TextLayoutFontData& fontData, TextLayoutScratch& scratch, double glyphScale)
        {
            scratch.Lines.Reset();
            const bool wrap = component.Wrapping && component.LayoutSize.x > 0.0f;
            const double widthLimit = std::max(0.0f, component.LayoutSize.x);
            const size_t tokenCount = scratch.Tokens.Size();
            size_t lineStart = 0;

            while (lineStart < tokenCount)
            {
                if (scratch.Tokens[lineStart].NewLine)
                {
                    AddLayoutLine(scratch, lineStart, lineStart, true);
                    lineStart++;
                    if (lineStart == tokenCount)
                        AddLayoutLine(scratch, lineStart, lineStart, true);
                    continue;
                }

                double pen = 0.0;
                size_t lastBreak = INVALID_TEXT_INDEX;
                size_t index = lineStart;
                bool emitted = false;
                while (index < tokenCount && !scratch.Tokens[index].NewLine)
                {
                    const TextLayoutToken& token = scratch.Tokens[index];
                    const double advance = TokenAdvance(token, pen, glyphScale, component, fontData);
                    const bool exceedsWidth = wrap && pen + advance > widthLimit + 0.000001;
                    const bool clusterStartsHere = index == lineStart || !TokensShareCluster(scratch.Tokens[index - 1], scratch.Tokens[index]);
                    const bool canBreakBeforeCurrent = component.WrapMode == TextWrapMode::Character ||
                                                       component.WrapMode == TextWrapMode::WordThenCharacter || lastBreak != INVALID_TEXT_INDEX;
                    if (exceedsWidth && clusterStartsHere && canBreakBeforeCurrent && index > lineStart)
                    {
                        size_t lineEnd = index;
                        size_t nextLine = index;
                        if (component.WrapMode != TextWrapMode::Character && lastBreak != INVALID_TEXT_INDEX && lastBreak > lineStart)
                        {
                            lineEnd = lastBreak;
                            nextLine = lastBreak;
                        }
                        while (nextLine < tokenCount && !scratch.Tokens[nextLine].NewLine &&
                               IsBreakableWhitespace(scratch.Tokens[nextLine].CodePoint))
                            nextLine++;
                        AddLayoutLine(scratch, lineStart, lineEnd, false);
                        lineStart = nextLine;
                        emitted = true;
                        break;
                    }

                    pen += advance;
                    index++;
                    if (token.BreakAfter)
                        lastBreak = index;

                    if (exceedsWidth && component.WrapMode == TextWrapMode::Word && token.BreakAfter)
                    {
                        AddLayoutLine(scratch, lineStart, index, false);
                        lineStart = index;
                        while (lineStart < tokenCount && !scratch.Tokens[lineStart].NewLine &&
                               IsBreakableWhitespace(scratch.Tokens[lineStart].CodePoint))
                            lineStart++;
                        emitted = true;
                        break;
                    }

                    if (exceedsWidth && component.WrapMode != TextWrapMode::Word)
                    {
                        index = AdvanceToClusterEnd(scratch, index);
                        AddLayoutLine(scratch, lineStart, index, false);
                        lineStart = index;
                        emitted = true;
                        break;
                    }
                }

                if (emitted)
                    continue;

                const bool paragraphEnd = index < tokenCount && scratch.Tokens[index].NewLine;
                AddLayoutLine(scratch, lineStart, index, true);
                lineStart = paragraphEnd ? index + 1 : index;
                if (paragraphEnd && lineStart == tokenCount)
                    AddLayoutLine(scratch, lineStart, lineStart, true);
            }
        }

        struct NaturalTextMetrics
        {
            double Width = 0.0;
            double Height = 0.0;
            double GlyphScale = 0.0;
            double LineAdvance = 0.0;
            bool OverflowX = false;
            bool OverflowY = false;
        };

        NaturalTextMetrics MeasureNaturalLayout(const TextComponent& component, const TextLayoutFontData& fontData, TextLayoutScratch& scratch,
                                                double fontSize)
        {
            NaturalTextMetrics result;
            const double metricHeight = fontData.Ascender - fontData.Descender;
            result.GlyphScale = (fontSize / 36.0) / metricHeight;
            result.LineAdvance = std::max(metricHeight * result.GlyphScale * 0.1, fontData.LineHeight * result.GlyphScale + component.LineSpacing);
            BuildLineRanges(component, fontData, scratch, result.GlyphScale);

            double baseline = 0.0;
            for (size_t index = 0; index < scratch.Lines.Size(); index++)
            {
                TextLayoutLine& line = scratch.Lines[index];
                line.Baseline = static_cast<float>(baseline);
                line.NaturalWidth =
                  static_cast<float>(MeasureTokenRange(scratch.Tokens, line.TokenStart, line.TokenEnd, result.GlyphScale, component, fontData));
                result.Width = std::max(result.Width, static_cast<double>(line.NaturalWidth));
                if (index + 1 < scratch.Lines.Size())
                {
                    baseline -= result.LineAdvance;
                    if (line.ParagraphEnd)
                        baseline -= component.ParagraphSpacing;
                }
            }

            if (!scratch.Lines.Empty())
            {
                const double bottom = scratch.Lines[scratch.Lines.Size() - 1].Baseline + fontData.Descender * result.GlyphScale;
                result.Height = fontData.Ascender * result.GlyphScale - bottom;
            }
            result.OverflowX = component.LayoutSize.x > 0.0f && result.Width > component.LayoutSize.x + 0.000001;
            result.OverflowY = component.LayoutSize.y > 0.0f && result.Height > component.LayoutSize.y + 0.000001;
            return result;
        }

        size_t FitTokenRange(const TextComponent& component, const TextLayoutFontData& fontData, const TextLayoutScratch& scratch,
                             const TextLayoutLine& line, double glyphScale, double widthLimit, bool reserveEllipsis)
        {
            const double ellipsisWidth = reserveEllipsis ? fontData.EllipsisAdvance * glyphScale : 0.0;
            const double available = std::max(0.0, widthLimit - ellipsisWidth);
            double pen = 0.0;
            size_t result = line.TokenStart;
            for (size_t index = line.TokenStart; index < line.TokenEnd; index++)
            {
                const TextLayoutToken& token = scratch.Tokens[index];
                const double advance = TokenAdvance(token, pen, glyphScale, component, fontData);
                const double candidatePen = pen + advance;
                const double candidateWidth =
                  !token.WhiteSpace && !token.Invisible ? std::max(0.0, candidatePen - static_cast<double>(component.CharacterSpacing)) : pen;
                if (candidateWidth > available + 0.000001)
                    break;
                pen = candidatePen;
                result = index + 1;
            }
            if (result < line.TokenEnd)
            {
                while (result > line.TokenStart && result < line.TokenEnd && TokensShareCluster(scratch.Tokens[result - 1], scratch.Tokens[result]))
                    result--;
            }
            return TrimTrailingWhitespace(scratch.Tokens, line.TokenStart, result);
        }

        uint32_t CountWhitespaceGaps(const TextLayoutScratch& scratch, size_t begin, size_t end)
        {
            uint32_t count = 0;
            for (size_t index = begin; index < end; index++)
            {
                if (!scratch.Tokens[index].WhiteSpace)
                    continue;
                const bool hasTextBefore = index > begin;
                const bool atEndOfRun = index + 1 == end || !scratch.Tokens[index + 1].WhiteSpace;
                if (hasTextBefore && atEndOfRun && index + 1 < end)
                    count++;
            }
            return count;
        }

        uint32_t CountInterCharacterGaps(const TextLayoutScratch& scratch, size_t begin, size_t end)
        {
            uint32_t glyphCount = 0;
            for (size_t index = begin; index < end; index++)
            {
                const TextLayoutToken& token = scratch.Tokens[index];
                if (token.Renderable && !token.WhiteSpace && !token.Invisible && !token.CombiningMark)
                    glyphCount++;
            }
            return glyphCount > 1 ? glyphCount - 1 : 0;
        }

        void AddGlyphToFontRun(TextLayoutScratch& scratch, const TextLayoutGlyph& glyph)
        {
            if (scratch.FontRuns.Empty() || scratch.FontRuns[scratch.FontRuns.Size() - 1].SourceFont != glyph.SourceFont)
            {
                TextLayoutFontRun& run = scratch.FontRuns.Acquire();
                run = {};
                run.SourceFont = glyph.SourceFont;
                run.FirstGlyph = scratch.Glyphs.Size() - 1;
            }
            scratch.FontRuns[scratch.FontRuns.Size() - 1].GlyphCount++;
        }
    } // namespace

    void TextLayout::DecodeUTF8(StringView text, TextLayoutScratch& scratch)
    {
        scratch.Reset();
        scratch.Reserve(text.size());
        scratch.SourceByteLength = text.size();

        size_t offset = 0;
        while (offset < text.size())
        {
            const size_t sourceByteStart = offset;
            char32_t codePoint = 0;
            if (!UTF8::NextCodePoint(text, offset, codePoint))
                break;

            if (codePoint == U'\n' && !scratch.Tokens.Empty() && scratch.Tokens[scratch.Tokens.Size() - 1].CodePoint == U'\n' &&
                text[sourceByteStart - 1] == '\r')
            {
                scratch.Tokens[scratch.Tokens.Size() - 1].SourceByteEnd = offset;
                continue;
            }

            if (codePoint == U'\r')
                codePoint = U'\n';

            TextLayoutToken& token = scratch.Tokens.Acquire();
            token = {};
            token.CodePoint = codePoint;
            token.SourceByteStart = sourceByteStart;
            token.SourceByteEnd = offset;
            token.NewLine = codePoint == U'\n';
            token.WhiteSpace = IsTextWhitespace(codePoint);
            token.Invisible = codePoint == 0x200B || codePoint == 0x00AD || codePoint == 0x200D || IsVariationSelector(codePoint);
            token.CombiningMark = IsCombiningMark(codePoint) || IsVariationSelector(codePoint) || IsEmojiModifier(codePoint) || codePoint == 0x200D;
        }

        AssignClusterRanges(scratch);
        for (size_t index = 0; index < scratch.Tokens.Size(); index++)
        {
            TextLayoutToken& token = scratch.Tokens[index];
            token.BreakAfter = IsBreakableWhitespace(token.CodePoint) || IsExplicitBreakCharacter(token.CodePoint);
            if (IsCJKCharacter(token.CodePoint) && index + 1 < scratch.Tokens.Size())
            {
                const TextLayoutToken& next = scratch.Tokens[index + 1];
                token.BreakAfter |= !next.NewLine && !next.CombiningMark && !IsClosingPunctuation(next.CodePoint);
            }
        }
    }

    TextLayoutResult TextLayout::Build(const TextComponent& component, const Font& font, TextLayoutScratch& scratch)
    {
        TextLayoutResult result;
        const msdfgen::FontMetrics* fontMetrics = font.GetMetrics();
        if (!font.IsValid() || fontMetrics == nullptr || component.Text.empty())
            return result;

        DecodeUTF8(component.Text, scratch);
        ResolveTokenFonts(component, font, scratch);
        const ResolvedGlyph space = ResolveGlyph(font, U' ');
        const ResolvedGlyph ellipsis = ResolveGlyph(font, 0x2026);
        const TextLayoutFontData fontData{ fontMetrics->ascenderY,
                                           fontMetrics->descenderY,
                                           fontMetrics->lineHeight,
                                           space.SourceFont != nullptr ? space.SourceFont->GetAdvance(space.CodePoint, 0, false) : 0.0,
                                           ellipsis.SourceFont != nullptr ? ellipsis.SourceFont->GetAdvance(ellipsis.CodePoint) : 0.0,
                                           ResolveTabWidth(component, font),
                                           ellipsis.Glyph,
                                           ellipsis.SourceFont };
        return BuildPrepared(component, fontData, scratch);
    }

    TextLayoutResult TextLayout::BuildPrepared(const TextComponent& component, const TextLayoutFontData& fontData, TextLayoutScratch& scratch)
    {
        TextLayoutResult result;
        const double metricHeight = fontData.Ascender - fontData.Descender;
        if (scratch.Tokens.Empty() || metricHeight <= std::numeric_limits<double>::epsilon())
            return result;

        double fontSize = std::max(0.0f, component.Size);
        if (component.AutoSize)
        {
            const double minimumSize = std::max(0.01f, std::min(component.AutoSizeMin, component.AutoSizeMax));
            const double maximumSize = std::max(minimumSize, static_cast<double>(std::max(component.AutoSizeMin, component.AutoSizeMax)));
            if (component.LayoutSize.x > 0.0f || component.LayoutSize.y > 0.0f)
            {
                double low = minimumSize;
                double high = maximumSize;
                for (uint32_t iteration = 0; iteration < 12; iteration++)
                {
                    const double candidate = (low + high) * 0.5;
                    const NaturalTextMetrics metrics = MeasureNaturalLayout(component, fontData, scratch, candidate);
                    const bool lineLimitFits = component.MaxLines == 0 || scratch.Lines.Size() <= component.MaxLines;
                    if (!metrics.OverflowX && !metrics.OverflowY && lineLimitFits)
                        low = candidate;
                    else
                        high = candidate;
                }
                fontSize = low;
            }
            else
                fontSize = std::clamp(static_cast<double>(component.Size), minimumSize, maximumSize);
        }
        if (fontSize <= 0.0)
            return result;

        const NaturalTextMetrics natural = MeasureNaturalLayout(component, fontData, scratch, fontSize);
        result.FontSize = static_cast<float>(fontSize);
        result.GlyphScale = static_cast<float>(natural.GlyphScale);
        result.LineAdvance = static_cast<float>(natural.LineAdvance);
        result.OverflowedHorizontally = natural.OverflowX;
        result.OverflowedVertically = natural.OverflowY;

        size_t visibleLineCount = scratch.Lines.Size();
        if (component.MaxLines > 0)
            visibleLineCount = std::min(visibleLineCount, static_cast<size_t>(component.MaxLines));

        if (component.Overflow != TextOverflow::Overflow && component.LayoutSize.y > 0.0f)
        {
            size_t fittingLines = 0;
            for (size_t index = 0; index < visibleLineCount; index++)
            {
                const double bottom = scratch.Lines[index].Baseline + fontData.Descender * natural.GlyphScale;
                const double height = fontData.Ascender * natural.GlyphScale - bottom;
                if (height > component.LayoutSize.y + 0.000001 && index > 0)
                    break;
                fittingLines = index + 1;
            }
            visibleLineCount = fittingLines;
        }

        const bool hiddenLines = visibleLineCount < scratch.Lines.Size();
        result.Truncated = hiddenLines;
        if (visibleLineCount == 0)
            return result;

        const double ellipsisWidth = fontData.EllipsisAdvance * natural.GlyphScale;
        double maximumLineWidth = 0.0;
        for (size_t lineIndex = 0; lineIndex < visibleLineCount; lineIndex++)
        {
            TextLayoutLine& line = scratch.Lines[lineIndex];
            line.RenderTokenEnd = TrimTrailingWhitespace(scratch.Tokens, line.TokenStart, line.TokenEnd);
            const bool horizontalOverflow = component.LayoutSize.x > 0.0f && line.NaturalWidth > component.LayoutSize.x + 0.000001;
            const bool verticalEllipsis = hiddenLines && lineIndex + 1 == visibleLineCount;
            line.Ellipsized = component.Overflow == TextOverflow::Ellipses && (horizontalOverflow || verticalEllipsis);

            if (component.LayoutSize.x > 0.0f && component.Overflow != TextOverflow::Overflow && (horizontalOverflow || line.Ellipsized))
            {
                line.RenderTokenEnd = FitTokenRange(component, fontData, scratch, line, natural.GlyphScale, component.LayoutSize.x, line.Ellipsized);
                result.Truncated |= line.RenderTokenEnd < TrimTrailingWhitespace(scratch.Tokens, line.TokenStart, line.TokenEnd);
            }

            line.Width =
              static_cast<float>(MeasureTokenRange(scratch.Tokens, line.TokenStart, line.RenderTokenEnd, natural.GlyphScale, component, fontData) +
                                 (line.Ellipsized ? ellipsisWidth : 0.0));
            const bool justify = component.LayoutSize.x > 0.0f && !line.Ellipsized &&
                                 (component.HorizontalAlignment == TextHorizontalAlignment::Flush ||
                                  (component.HorizontalAlignment == TextHorizontalAlignment::Justified && !line.ParagraphEnd));
            if (justify && line.Width < component.LayoutSize.x)
            {
                line.ExpandableGaps = CountWhitespaceGaps(scratch, line.TokenStart, line.RenderTokenEnd);
                if (line.ExpandableGaps == 0)
                    line.ExpandableGaps = CountInterCharacterGaps(scratch, line.TokenStart, line.RenderTokenEnd);
                if (line.ExpandableGaps > 0)
                    line.Width = component.LayoutSize.x;
            }

            const bool boundedWidth = component.LayoutSize.x > 0.0f;
            const double horizontalSpace = boundedWidth ? component.LayoutSize.x : 0.0;
            switch (component.HorizontalAlignment)
            {
            case TextHorizontalAlignment::Center:
                line.X = static_cast<float>((horizontalSpace - line.Width) * 0.5);
                break;
            case TextHorizontalAlignment::Right:
                line.X = static_cast<float>(horizontalSpace - line.Width);
                break;
            default:
                line.X = 0.0f;
                break;
            }
            maximumLineWidth = std::max(maximumLineWidth, static_cast<double>(line.Width));
        }

        const double blockTop = fontData.Ascender * natural.GlyphScale;
        const double blockBottom = scratch.Lines[visibleLineCount - 1].Baseline + fontData.Descender * natural.GlyphScale;
        const double blockHeight = blockTop - blockBottom;
        const bool boundedHeight = component.LayoutSize.y > 0.0f;
        double verticalOffset = 0.0;
        switch (component.VerticalAlignment)
        {
        case TextVerticalAlignment::Top:
            verticalOffset = -blockTop;
            break;
        case TextVerticalAlignment::Middle:
            verticalOffset = (boundedHeight ? -component.LayoutSize.y * 0.5 : 0.0) - (blockTop + blockBottom) * 0.5;
            break;
        case TextVerticalAlignment::Bottom:
            verticalOffset = (boundedHeight ? -component.LayoutSize.y : 0.0) - blockBottom;
            break;
        case TextVerticalAlignment::Midline:
            verticalOffset =
              (boundedHeight ? -component.LayoutSize.y * 0.5 : 0.0) - (fontData.Ascender + fontData.Descender) * natural.GlyphScale * 0.5;
            break;
        case TextVerticalAlignment::Baseline:
        default:
            break;
        }
        for (size_t index = 0; index < visibleLineCount; index++)
            scratch.Lines[index].Baseline += static_cast<float>(verticalOffset);

        scratch.Glyphs.Reset();
        scratch.Carets.Reset();
        scratch.FontRuns.Reset();
        for (size_t lineIndex = 0; lineIndex < visibleLineCount; lineIndex++)
        {
            TextLayoutLine& line = scratch.Lines[lineIndex];
            line.FirstGlyph = scratch.Glyphs.Size();
            line.FirstCaret = scratch.Carets.Size();
            double pen = line.X;
            const double relativeWidth = line.Width;
            const double naturalOutputWidth =
              MeasureTokenRange(scratch.Tokens, line.TokenStart, line.RenderTokenEnd, natural.GlyphScale, component, fontData) +
              (line.Ellipsized ? ellipsisWidth : 0.0);
            const double gapExpansion = line.ExpandableGaps > 0 ? (relativeWidth - naturalOutputWidth) / line.ExpandableGaps : 0.0;
            const bool whitespaceGaps = CountWhitespaceGaps(scratch, line.TokenStart, line.RenderTokenEnd) > 0;
            uint32_t expandedCharacterGaps = 0;

            const size_t initialSourceByteOffset =
              line.TokenStart < scratch.Tokens.Size() ? scratch.Tokens[line.TokenStart].SourceByteStart : scratch.SourceByteLength;
            TextLayoutCaret& initialCaret = scratch.Carets.Acquire();
            initialCaret.SourceByteOffset = initialSourceByteOffset;
            initialCaret.Position = { static_cast<float>(pen), line.Baseline };
            initialCaret.LineIndex = static_cast<uint32_t>(lineIndex);

            for (size_t index = line.TokenStart; index < line.RenderTokenEnd; index++)
            {
                const TextLayoutToken& token = scratch.Tokens[index];
                const double advance = TokenAdvance(token, pen - line.X, natural.GlyphScale, component, fontData);
                if (token.Renderable && !token.WhiteSpace && !token.Invisible)
                {
                    TextLayoutGlyph& glyph = scratch.Glyphs.Acquire();
                    glyph.CodePoint = token.ResolvedCodePoint != 0 ? token.ResolvedCodePoint : token.CodePoint;
                    glyph.Glyph = token.Glyph;
                    glyph.SourceFont = token.SourceFont;
                    glyph.PenPosition = { static_cast<float>(pen), line.Baseline };
                    glyph.Advance = static_cast<float>(advance);
                    glyph.LineIndex = static_cast<uint32_t>(lineIndex);
                    AddGlyphToFontRun(scratch, glyph);
                }
                pen += advance;

                if (gapExpansion > 0.0)
                {
                    if (whitespaceGaps)
                    {
                        const bool endOfWhitespaceRun = token.WhiteSpace && index + 1 < line.RenderTokenEnd && !scratch.Tokens[index + 1].WhiteSpace;
                        if (endOfWhitespaceRun)
                            pen += gapExpansion;
                    }
                    else if (token.Renderable && !token.CombiningMark && expandedCharacterGaps < line.ExpandableGaps)
                    {
                        pen += gapExpansion;
                        expandedCharacterGaps++;
                    }
                }

                const bool clusterContinues = index + 1 < line.RenderTokenEnd &&
                                              (scratch.Tokens[index + 1].CombiningMark || TokensShareCluster(token, scratch.Tokens[index + 1]));
                if (!clusterContinues)
                {
                    TextLayoutCaret& caret = scratch.Carets.Acquire();
                    caret.SourceByteOffset = token.ClusterByteEnd > token.ClusterByteStart ? token.ClusterByteEnd : token.SourceByteEnd;
                    caret.Position = { static_cast<float>(pen), line.Baseline };
                    caret.LineIndex = static_cast<uint32_t>(lineIndex);
                }
            }

            if (line.Ellipsized)
            {
                TextLayoutGlyph& glyph = scratch.Glyphs.Acquire();
                glyph.CodePoint = 0x2026;
                glyph.Glyph = fontData.EllipsisGlyph;
                glyph.SourceFont = fontData.EllipsisSourceFont;
                glyph.PenPosition = { static_cast<float>(pen), line.Baseline };
                glyph.Advance = static_cast<float>(ellipsisWidth);
                glyph.LineIndex = static_cast<uint32_t>(lineIndex);
                AddGlyphToFontRun(scratch, glyph);

                TextLayoutCaret& caret = scratch.Carets.Acquire();
                caret.SourceByteOffset =
                  line.RenderTokenEnd > line.TokenStart ? scratch.Tokens[line.RenderTokenEnd - 1].SourceByteEnd : initialSourceByteOffset;
                caret.Position = { static_cast<float>(pen + ellipsisWidth), line.Baseline };
                caret.LineIndex = static_cast<uint32_t>(lineIndex);
            }
            line.GlyphCount = scratch.Glyphs.Size() - line.FirstGlyph;
            line.CaretCount = scratch.Carets.Size() - line.FirstCaret;
        }

        result.Glyphs = scratch.Glyphs.Empty() ? nullptr : scratch.Glyphs.begin();
        result.Lines = scratch.Lines.begin();
        result.Carets = scratch.Carets.Empty() ? nullptr : scratch.Carets.begin();
        result.FontRuns = scratch.FontRuns.Empty() ? nullptr : scratch.FontRuns.begin();
        result.GlyphCount = scratch.Glyphs.Size();
        result.LineCount = visibleLineCount;
        result.CaretCount = scratch.Carets.Size();
        result.FontRunCount = scratch.FontRuns.Size();
        result.Size = { static_cast<float>(maximumLineWidth), static_cast<float>(blockHeight) };
        return result;
    }

    TextHitTestResult TextLayout::HitTest(const TextLayoutResult& layout, const glm::vec2& position)
    {
        TextHitTestResult result;
        if (layout.Lines == nullptr || layout.Carets == nullptr || layout.LineCount == 0 || layout.CaretCount == 0)
            return result;

        const TextLayoutLine* closestLine = nullptr;
        float closestLineDistance = std::numeric_limits<float>::max();
        for (size_t lineIndex = 0; lineIndex < layout.LineCount; lineIndex++)
        {
            const TextLayoutLine& line = layout.Lines[lineIndex];
            if (line.CaretCount == 0)
                continue;

            const float distance = std::abs(position.y - line.Baseline);
            if (distance < closestLineDistance)
            {
                closestLineDistance = distance;
                closestLine = &line;
            }
        }
        if (closestLine == nullptr)
            return result;

        const TextLayoutCaret* begin = layout.Carets + closestLine->FirstCaret;
        const TextLayoutCaret* end = begin + closestLine->CaretCount;
        const TextLayoutCaret* upper =
          std::lower_bound(begin, end, position.x, [](const TextLayoutCaret& caret, float x) { return caret.Position.x < x; });

        const TextLayoutCaret* closest = nullptr;
        if (upper == begin)
            closest = begin;
        else if (upper == end)
            closest = end - 1;
        else
        {
            const TextLayoutCaret* lower = upper - 1;
            closest = position.x - lower->Position.x < upper->Position.x - position.x ? lower : upper;
        }

        result.SourceByteOffset = closest->SourceByteOffset;
        result.CaretPosition = closest->Position;
        result.LineIndex = closest->LineIndex;
        result.Valid = true;
        return result;
    }
} // namespace Crowny
