#include "cwpch.h"

#include "Crowny/Import/FontImporter.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Timer.h"
#include "Crowny/Common/UTF8.h"
#include "Crowny/Import/ImportOptions.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/MSDFdata.h"
#include "Crowny/Utils/PixelUtils.h"

#include <msdf-atlas-gen.h>

#include <charconv>
#include <limits>

#define DEFAULT_ANGLE_THRESHOLD 3.0
#define LCG_MULTIPLIER 6364136223846793005ull
#define LCG_INCREMENT 1442695040888963407ull

namespace Crowny
{
    namespace
    {
        constexpr uint32_t MAX_CUSTOM_CHARACTERS = 65536;

        uint32_t GetFontWorkerCount()
        {
            const uint32_t hardwareThreads = std::thread::hardware_concurrency();
            return hardwareThreads > 2 ? hardwareThreads - 2 : 1;
        }

        bool IsValidCodePoint(uint32_t codePoint)
        {
            return codePoint <= 0x10FFFF && !(codePoint >= 0xD800 && codePoint <= 0xDFFF);
        }

        bool ParseCodePoint(StringView token, int base, uint32_t& codePoint)
        {
            if (token.starts_with("U+") || token.starts_with("u+"))
                token.remove_prefix(2);
            else if (token.starts_with("0x") || token.starts_with("0X"))
                token.remove_prefix(2);
            if (token.empty())
                return false;

            const auto result = std::from_chars(token.data(), token.data() + token.size(), codePoint, base);
            return result.ec == std::errc() && result.ptr == token.data() + token.size() && IsValidCodePoint(codePoint);
        }

        void AddNumericCharset(msdf_atlas::Charset& charset, StringView source, int base)
        {
            String normalized(source);
            for (char& character : normalized)
            {
                if (character == ',' || character == ';' || character == '\n' || character == '\r' || character == '\t')
                    character = ' ';
            }

            String token;
            StringStream stream(normalized);
            while (stream >> token && charset.size() < MAX_CUSTOM_CHARACTERS)
            {
                const size_t rangeSeparator = token.find('-');
                uint32_t first = 0;
                if (rangeSeparator == String::npos)
                {
                    if (ParseCodePoint(token, base, first))
                        charset.add(first);
                    continue;
                }

                uint32_t last = 0;
                if (!ParseCodePoint(StringView(token).substr(0, rangeSeparator), base, first) ||
                    !ParseCodePoint(StringView(token).substr(rangeSeparator + 1), base, last))
                    continue;
                if (last < first)
                    std::swap(first, last);
                for (uint32_t codePoint = first; codePoint <= last && charset.size() < MAX_CUSTOM_CHARACTERS; codePoint++)
                {
                    if (IsValidCodePoint(codePoint))
                        charset.add(codePoint);
                    if (codePoint == 0x10FFFF)
                        break;
                }
            }
        }

        void AddLiteralCharset(msdf_atlas::Charset& charset, StringView source)
        {
            size_t offset = 0;
            char32_t codePoint = 0;
            while (charset.size() < MAX_CUSTOM_CHARACTERS && UTF8::NextCodePoint(source, offset, codePoint))
                charset.add(codePoint);
        }

        void AddRange(msdf_atlas::Charset& charset, char32_t first, char32_t last)
        {
            for (char32_t codePoint = first; codePoint <= last; codePoint++)
                charset.add(codePoint);
        }

        msdf_atlas::Charset BuildCharset(const FontImportOptions& options)
        {
            msdf_atlas::Charset charset;
            switch (options.Range)
            {
            case CharsetRange::ASCII:
                AddRange(charset, 32, 126);
                break;
            case CharsetRange::ExtendedASCII:
                AddRange(charset, 32, 255);
                AddRange(charset, 8192, 8303);
                charset.add(8364);
                charset.add(8482);
                break;
            case CharsetRange::LowerASCII:
                AddRange(charset, 32, 64);
                AddRange(charset, 91, 126);
                break;
            case CharsetRange::UpperASCII:
                AddRange(charset, 32, 96);
                AddRange(charset, 123, 126);
                break;
            case CharsetRange::NumbersAndSymbols:
                AddRange(charset, 32, 64);
                AddRange(charset, 91, 96);
                AddRange(charset, 123, 126);
                break;
            case CharsetRange::SymbolRange:
                AddLiteralCharset(charset, options.CustomCharset);
                break;
            case CharsetRange::DecimalRange:
                AddNumericCharset(charset, options.CustomCharset, 10);
                break;
            case CharsetRange::HexRange:
                AddNumericCharset(charset, options.CustomCharset, 16);
                break;
            default:
                AddRange(charset, 32, 126);
                break;
            }

            charset.add(U' ');
            charset.add(U'?');
            charset.add(0xA0);
            charset.add(0x200B);
            charset.add(0x2026);
            charset.add(0x25A1);
            charset.add(0xFFFD);
            return charset;
        }

        msdf_atlas::TightAtlasPacker::DimensionsConstraint GetAtlasConstraint(Font::AtlasDimensionsConstraint constraint)
        {
            using Destination = msdf_atlas::TightAtlasPacker::DimensionsConstraint;
            switch (constraint)
            {
            case Font::AtlasDimensionsConstraint::POWER_OF_TWO_RECTANGLE:
                return Destination::POWER_OF_TWO_RECTANGLE;
            case Font::AtlasDimensionsConstraint::MULTIPLE_OF_FOUR_SQUARE:
                return Destination::MULTIPLE_OF_FOUR_SQUARE;
            case Font::AtlasDimensionsConstraint::EVEN_SQUARE:
                return Destination::EVEN_SQUARE;
            case Font::AtlasDimensionsConstraint::SQUARE:
                return Destination::SQUARE;
            default:
                return Destination::POWER_OF_TWO_SQUARE;
            }
        }

        struct FontLibraryScope
        {
            ~FontLibraryScope()
            {
                if (Font != nullptr)
                    msdfgen::destroyFont(Font);
                if (FreeType != nullptr)
                    msdfgen::deinitializeFreetype(FreeType);
            }

            msdfgen::FreetypeHandle* FreeType = nullptr;
            msdfgen::FontHandle* Font = nullptr;
        };
    } // namespace

    template <typename T, typename S, int N, msdf_atlas::GeneratorFunction<S, N> GenFunc>
    Ref<Texture> CreateAtlas(const Vector<msdf_atlas::GlyphGeometry>& glyphs, uint32_t width, uint32_t height, uint32_t workerCount)
    {
        msdf_atlas::GeneratorAttributes generatorAttributes;
        generatorAttributes.config.overlapSupport = true;
        generatorAttributes.scanlinePass = true;

        msdf_atlas::ImmediateAtlasGenerator<S, N, GenFunc, msdf_atlas::BitmapAtlasStorage<T, N>> generator(width, height);
        generator.setAttributes(generatorAttributes);
        generator.setThreadCount(workerCount);
        generator.generate(glyphs.data(), (int)glyphs.size());

        msdfgen::BitmapConstRef<T, N> bitmap = (msdfgen::BitmapConstRef<T, N>)generator.atlasStorage();

        TextureDesc params;
        params.Width = width;
        params.Height = height;
        params.Format = TextureFormat::RGB8;
        params.GenerateMipmaps = false;

        PixelData pixelData(width, height, 1, params.Format);
        Ref<Texture> texture = Texture::Create(params);
        pixelData.SetBuffer((uint8_t*)bitmap.pixels);
        texture->WriteData(pixelData);
        texture->SetName("Font Atlas");

        return texture;
    }

    bool FontImporter::IsExtensionSupported(const String& extension) const
    {
        return extension == "ttf" || extension == "ttc" || extension == "otf" || extension == "otc" || extension == "fnt";
    }

    bool FontImporter::IsMagicNumSupported(uint8_t* num, uint32_t numSize) const { return true; }

    Ref<Asset> FontImporter::Import(const Path& path, Ref<const ImportOptions> importOptions)
    {
        const Ref<const FontImportOptions> options = StaticRefCast<const FontImportOptions>(importOptions);
        if (!options)
        {
            CW_ENGINE_ERROR("Font import options are missing for '{}'.", path);
            return nullptr;
        }

        FontLibraryScope fontLibrary;
        fontLibrary.FreeType = msdfgen::initializeFreetype();
        if (fontLibrary.FreeType == nullptr)
        {
            CW_ENGINE_ERROR("Could not initialize FreeType while importing '{}'.", path);
            return nullptr;
        }

        const Ref<DataStream> dataStream = FileSystem::OpenFile(path);
        if (!dataStream || dataStream->Size() == 0 || dataStream->Size() > static_cast<size_t>(std::numeric_limits<int>::max()))
        {
            CW_ENGINE_ERROR("Font source '{}' is empty, missing, or too large.", path);
            return nullptr;
        }

        Vector<uint8_t> data(dataStream->Size());
        const size_t bytesRead = dataStream->Read(data.data(), data.size());
        dataStream->Close();
        if (bytesRead != data.size())
        {
            CW_ENGINE_ERROR("Could not read all bytes from font source '{}'.", path);
            return nullptr;
        }

        fontLibrary.Font = msdfgen::loadFontData(fontLibrary.FreeType, data.data(), static_cast<int>(data.size()));
        if (fontLibrary.Font == nullptr)
        {
            CW_ENGINE_ERROR("Could not load font '{}'.", path.filename().string());
            return nullptr;
        }

        msdfgen::FontImportInfo fontImportInfo;
        if (msdfgen::getImportInfo(fontImportInfo, fontLibrary.Font))
            CW_ENGINE_INFO("Font family: {}, font style: {}", fontImportInfo.fontFamilyName, fontImportInfo.fontStyleName);
        msdfgen::FontMetrics fontMetrics;
        if (msdfgen::getFontMetrics(fontMetrics, fontLibrary.Font))
            CW_ENGINE_INFO("Font metrics: em {}, ascender {}, descender {}, line height {}", fontMetrics.emSize, fontMetrics.ascenderY,
                           fontMetrics.descenderY, fontMetrics.lineHeight);

        if (options->DynamicFontAtlas)
            CW_ENGINE_WARN("Dynamic font atlases are not implemented. Importing '{}' as a static atlas.", path.filename().string());

        const msdf_atlas::Charset charset = BuildCharset(*options);
        Scope<MSDFData> fontData = CreateScope<MSDFData>();
        fontData->FontGeometry = msdf_atlas::FontGeometry(&fontData->Glyphs);
        const int glyphsLoaded =
          fontData->FontGeometry.loadCharset(fontLibrary.Font, 1.0, charset, true, options->GetKerningData);
        if (glyphsLoaded <= 0)
        {
            CW_ENGINE_ERROR("Font '{}' did not provide any requested glyphs.", path.filename().string());
            return nullptr;
        }
        if (glyphsLoaded < charset.size())
            CW_ENGINE_WARN("Loaded {} of {} requested glyphs from '{}'.", glyphsLoaded, charset.size(), path.filename().string());
        else
            CW_ENGINE_INFO("Loaded {} glyphs from '{}'.", glyphsLoaded, path.filename().string());

        msdf_atlas::TightAtlasPacker atlasPacker;
        atlasPacker.setMiterLimit(1.0);
        atlasPacker.setPixelRange(2.0);
        atlasPacker.setPadding(static_cast<int>(std::min(options->Padding, 256U)));
        const double requestedScale = static_cast<double>(std::clamp(options->SamplingFontSize, 4U, 512U));
        if (options->AutoSizeAtlas)
        {
            atlasPacker.setDimensionsConstraint(GetAtlasConstraint(options->AtlasDimensionsConstraint));
            atlasPacker.setScale(requestedScale);
        }
        else
        {
            atlasPacker.setDimensions(static_cast<int>(std::clamp(options->AtlasWidth, 4U, 16384U)),
                                      static_cast<int>(std::clamp(options->AtlasHeight, 4U, 16384U)));
            if (!options->AutomaticFontSampling)
                atlasPacker.setScale(requestedScale);
        }

        Timer timer;
        const int remainingGlyphs = atlasPacker.pack(fontData->Glyphs.data(), static_cast<int>(fontData->Glyphs.size()));
        if (remainingGlyphs != 0)
        {
            CW_ENGINE_ERROR("Could not fit {} glyphs in the atlas for '{}'.", remainingGlyphs, path.filename().string());
            return nullptr;
        }

        int width = 0;
        int height = 0;
        atlasPacker.getDimensions(width, height);
        if (width <= 0 || height <= 0 || width > 16384 || height > 16384)
        {
            CW_ENGINE_ERROR("Font atlas dimensions {}x{} are invalid for '{}'.", width, height, path.filename().string());
            return nullptr;
        }
        CW_ENGINE_INFO("Packed font atlas {}x{} at scale {} in {}s.", width, height, atlasPacker.getScale(), timer.ElapsedSeconds());

        const uint32_t workerCount = GetFontWorkerCount();
        const uint64_t coloringSeed = 0;
        timer.Reset();
        msdf_atlas::Workload(
          [&glyphs = fontData->Glyphs, coloringSeed](int index, int) -> bool {
              const uint64_t glyphSeed = (LCG_MULTIPLIER * (coloringSeed ^ index) + LCG_INCREMENT) * !!coloringSeed;
              glyphs[index].edgeColoring(msdfgen::edgeColoringInkTrap, DEFAULT_ANGLE_THRESHOLD, glyphSeed);
              return true;
          },
          static_cast<int>(fontData->Glyphs.size()))
          .finish(workerCount);
        CW_ENGINE_INFO("Colored font edges in {}s.", timer.ElapsedSeconds());

        timer.Reset();
        const Ref<Texture> atlasTexture =
          CreateAtlas<uint8_t, float, 3, msdf_atlas::msdfGenerator>(fontData->Glyphs, width, height, workerCount);
        if (!atlasTexture)
        {
            CW_ENGINE_ERROR("Could not create the atlas texture for '{}'.", path.filename().string());
            return nullptr;
        }
        CW_ENGINE_INFO("Generated font atlas in {}s.", timer.ElapsedSeconds());

        const String fontFilename = path.filename().string();
        const Ref<Font> font = CreateRef<Font>(fontData.release(), atlasTexture, std::max(1U, options->TabMultiple));
        font->SetName(fontFilename);
        return font;
    }

    Ref<ImportOptions> FontImporter::CreateImportOptions() const { return CreateRef<FontImportOptions>(); }
} // namespace Crowny
