#include "cwpch.h"

#include "Crowny/Import/FontImporter.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Common/Timer.h"
#include "Crowny/Import/ImportOptions.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/MSDFdata.h"
#include "Crowny/Utils/PixelUtils.h"

#include <msdf-atlas-gen.h>

#define DEFAULT_ANGLE_THRESHOLD 3.0
#define LCG_MULTIPLIER 6364136223846793005ull
#define LCG_INCREMENT 1442695040888963407ull

namespace Crowny
{
    template <typename T, typename S, int N, msdf_atlas::GeneratorFunction<S, N> GenFunc>
    const Ref<Texture> CreateAndCacheAtlas(const String& fontName, float fontSize,
                                           const Vector<msdf_atlas::GlyphGeometry>& glyphs,
                                           const msdf_atlas::FontGeometry& fontGeometry, uint32_t width,
                                           uint32_t height)
    {
        msdf_atlas::GeneratorAttributes generatorAttributes;
        generatorAttributes.config.overlapSupport = true;
        generatorAttributes.scanlinePass = true;

        msdf_atlas::ImmediateAtlasGenerator<S, N, GenFunc, msdf_atlas::BitmapAtlasStorage<T, N>> generator(width,
                                                                                                           height);
        generator.setAttributes(generatorAttributes);
        generator.setThreadCount(std::thread::hardware_concurrency() - 2);
        generator.generate(glyphs.data(), (int)glyphs.size());

        msdfgen::BitmapConstRef<T, N> bitmap = (msdfgen::BitmapConstRef<T, N>)generator.atlasStorage();

        TextureParameters params;
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
        String lower = extension;
        StringUtils::ToLower(lower);
        return extension == "ttf" || extension == "ttc" || extension == "otf" || extension == "otc" ||
               extension == "fnt";
    }

    bool FontImporter::IsMagicNumSupported(uint8_t* num, uint32_t numSize) const { return true; }

    Ref<Asset> FontImporter::Import(const Path& path, Ref<const ImportOptions> importOptions)
    {
        msdfgen::FreetypeHandle* freetypeHandle = msdfgen::initializeFreetype();
        if (freetypeHandle == nullptr)
        {
            CW_ENGINE_ERROR("Couldn't initialize FreeType, font import failed");
            return nullptr;
        }

        Ref<const FontImportOptions> fontImportOptions =
          std::static_pointer_cast<const FontImportOptions>(importOptions);
        std::vector<uint8_t> data;
        Ref<DataStream> dataStream = FileSystem::OpenFile(path);
        data.resize(dataStream->Size());
        dataStream->Read(data.data(), data.size());
        dataStream->Close();

        msdfgen::FontHandle* fontHandle = msdfgen::loadFontData(freetypeHandle, data.data(), (int)data.size());
        msdfgen::FontImportInfo fontImportInfo;
        msdfgen::getImportInfo(fontImportInfo, fontHandle);
        CW_ENGINE_INFO("Font family: {}, font style: {}", fontImportInfo.fontFamilyName, fontImportInfo.fontStyleName);
        msdfgen::FontMetrics fontMetrics;
        msdfgen::getFontMetrics(fontMetrics, fontHandle);
        CW_ENGINE_INFO("Font emSize: {}, AscenderY: {}, DescenderY {}, Line Height: {}, StrikethroughtY: {}, "
                       "UnderlineU: {}, UnderlineThickness: {}",
                       fontMetrics.emSize, fontMetrics.ascenderY, fontMetrics.descenderY, fontMetrics.lineHeight,
                       fontMetrics.strikethroughY, fontMetrics.underlineY, fontMetrics.underlineThickness);

        if (fontHandle == nullptr)
        {
            CW_ENGINE_ERROR("Couldn't load font: {}", path.filename().string());
            return nullptr;
        }

        if (!fontImportOptions->DynamicFontAtlas)
        {
            msdf_atlas::Charset charset;
            // Create the charset
            if (fontImportOptions->Range == CharsetRange::ASCII)
            {
                for (char32_t cp = 32; cp <= 126; cp++)
                    charset.add(cp);
                charset.add(160);
                charset.add(8203);
                charset.add(8230);
                charset.add(9633);
            }
            if (fontImportOptions->Range == CharsetRange::LowerASCII)
            {
                for (char32_t cp = 32; cp <= 64; cp++)
                    charset.add(cp);
                for (char32_t cp = 91; cp <= 126; cp++)
                    charset.add(cp);
                charset.add(160);
            }
            if (fontImportOptions->Range == CharsetRange::UpperASCII)
            {
                for (char32_t cp = 32; cp <= 96; ++cp)
                    charset.add(cp);
                for (char32_t cp = 123; cp <= 126; ++cp)
                    charset.add(cp);
                charset.add(160);
            }
            else if (fontImportOptions->Range == CharsetRange::ExtendedASCII)
            {
                for (char32_t cp = 32; cp <= 126; cp++)
                    charset.add(cp);
                for (char32_t cp = 160; cp <= 255; cp++)
                    charset.add(cp);
                for (char32_t cp = 8192; cp <= 8303; cp++)
                    charset.add(cp);
                charset.add(8364);
                charset.add(8482);
                charset.add(9632);
                charset.add(9633);
            }
            else if (fontImportOptions->Range == CharsetRange::NumbersAndSymbols)
            {
                for (char32_t cp = 32; cp <= 64; cp++)
                    charset.add(cp);
                for (char32_t cp = 91; cp <= 96; cp++)
                    charset.add(cp);
                for (char32_t cp = 123; cp <= 126; cp++)
                    charset.add(cp);
                charset.add(160);
            }
            else if (fontImportOptions->Range == CharsetRange::SymbolRange)
            {
                for (char32_t c : fontImportOptions->CustomCharset)
                    charset.add(c);
            }

            MSDFData* fontData = new MSDFData();

            const double fontScale = 1.0;
            fontData->FontGeometry = msdf_atlas::FontGeometry(&fontData->Glyphs);
            int glyphsLoaded = fontData->FontGeometry.loadCharset(fontHandle, fontScale, charset);
            if (glyphsLoaded < charset.size())
                CW_ENGINE_INFO("Loaded {} glyphs from font out of {}", glyphsLoaded, charset.size());
            else
                CW_ENGINE_INFO("Loaded {} glyphs from font", glyphsLoaded);

            msdf_atlas::TightAtlasPacker tightAtlasPacker;
            tightAtlasPacker.setMiterLimit(1.0);
            tightAtlasPacker.setPixelRange(2.0);

            tightAtlasPacker.setPadding(fontImportOptions->Padding);
            if (!fontImportOptions->AutoSizeAtlas)
                tightAtlasPacker.setDimensions(fontImportOptions->AtlasWidth, fontImportOptions->AtlasHeight);
            else
                tightAtlasPacker.setDimensionsConstraint(
                  msdf_atlas::TightAtlasPacker::DimensionsConstraint::MULTIPLE_OF_FOUR_SQUARE);

            if (!fontImportOptions->AutomaticFontSampling)
                tightAtlasPacker.setScale(fontImportOptions->SampingFontSize);

            Timer msdfTimer;
            int remainigGlyphs = tightAtlasPacker.pack(fontData->Glyphs.data(), (int)fontData->Glyphs.size());
            if (remainigGlyphs > 0)
                CW_ENGINE_ERROR("Couldn't fit {} glyphs in font atlas for font: {}", remainigGlyphs, path);
            CW_ENGINE_INFO("Font packing time: {}s", msdfTimer.ElapsedSeconds());

            int width = 0, height = 0;
            tightAtlasPacker.getDimensions(width, height);
            CW_ENGINE_INFO("Final atlas width: {0}, height: {1}", width, height);
            double scale = 0.0f;
            scale = tightAtlasPacker.getScale();
            CW_ENGINE_INFO("Final atlas font scale: {}", scale);

            uint64_t coloringSeed = 0;
            bool expensiveColoring = false;
            msdfTimer.Reset();
            if (expensiveColoring)
            {
                msdf_atlas::Workload(
                  [&glyphs = fontData->Glyphs, &coloringSeed](int idx, int threadIdx) -> bool {
                      uint64_t glyphSeed = (LCG_MULTIPLIER * (coloringSeed ^ idx) + LCG_INCREMENT) * !!coloringSeed;
                      glyphs[idx].edgeColoring(msdfgen::edgeColoringByDistance, DEFAULT_ANGLE_THRESHOLD, glyphSeed);
                      return true;
                  },
                  (int)fontData->Glyphs.size())
                  .finish(std::thread::hardware_concurrency() - 2);
            }
            else
            {
                msdf_atlas::Workload(
                  [&glyphs = fontData->Glyphs, &coloringSeed](int idx, int threadIdx) -> bool {
                      uint64_t glyphSeed = (LCG_MULTIPLIER * (coloringSeed ^ idx) + LCG_INCREMENT) * !!coloringSeed;
                      glyphs[idx].edgeColoring(msdfgen::edgeColoringInkTrap, DEFAULT_ANGLE_THRESHOLD, glyphSeed);
                      return true;
                  },
                  (int)fontData->Glyphs.size())
                  .finish(std::thread::hardware_concurrency() - 2);
            }
            CW_ENGINE_INFO("Font edge coloring took: {}s", msdfTimer.ElapsedSeconds());
            msdfTimer.Reset();
            Ref<Texture> atlasTexture = CreateAndCacheAtlas<uint8_t, float, 3, msdf_atlas::msdfGenerator>(
              path.filename().string(), (float)scale, fontData->Glyphs, fontData->FontGeometry, width, height);
            CW_ENGINE_INFO("Atlas generation took: {}s", msdfTimer.ElapsedSeconds());

            msdfgen::destroyFont(fontHandle);
            msdfgen::deinitializeFreetype(freetypeHandle);

            Ref<Font> font = CreateRef<Font>(fontData, atlasTexture);
            font->SetName(path.filename().string());
            return font;
        }
        // TODO: Implement for non-dynamic fonts.

        return nullptr;
    }

    Ref<ImportOptions> FontImporter::CreateImportOptions() const { return CreateRef<FontImportOptions>(); }
} // namespace Crowny