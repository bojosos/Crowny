#include "cwpch.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Import/FontImporter.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Renderer/FontManager.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace Crowny;

namespace
{
    AssetHandle<Font> CreateFontHandle(AssetManager& assetManager)
    {
        return static_asset_cast<Font>(assetManager.CreateAssetHandle(CreateRef<Font>()));
    }
} // namespace

TEST_CASE("An empty font has safe metrics and glyph queries", "[Renderer][Font]")
{
    Font font;
    FontMetrics metrics;

    CHECK_FALSE(font.IsValid());
    CHECK_FALSE(font.TryGetMetrics(metrics));
    CHECK(font.GetGlyphCount() == 0);
    CHECK_FALSE(font.HasGlyph(U'A'));
    CHECK(font.FindGlyph(U'A') == nullptr);
    CHECK_FALSE(font.ResolveGlyph(U'A'));
    const CharacterInfo character = font.GetCharacterInfo(U'A');
    CHECK_FALSE(character.Valid);
    CHECK(character.RequestedCodePoint == U'A');
    CHECK(character.ResolvedCodePoint == 0);
    CHECK(character.Advance == Catch::Approx(0.0));
    CHECK(font.GetAdvance(U'A') == Catch::Approx(0.0));
    CHECK(font.GetAtlasWidth() == 0);
    CHECK(font.GetAtlasHeight() == 0);
    CHECK(font.GetAtlasPixelRange() == Catch::Approx(2.0f));
}

TEST_CASE("Font fallback chains ignore unusable and duplicate entries", "[Renderer][Font][Fallback]")
{
    AssetManager assetManager;
    const AssetHandle<Font> primary = CreateFontHandle(assetManager);
    const AssetHandle<Font> fallback = CreateFontHandle(assetManager);
    const AssetHandle<Font> unloaded = static_asset_cast<Font>(assetManager.GetAssetHandle(UUID("10000000-0000-0000-0000-000000000001")));

    primary->SetFallbackFonts({ {}, primary, fallback, fallback });

    REQUIRE(primary->GetFallbackFonts().size() == 1);
    CHECK(primary->GetFallbackFonts().front().Get() == fallback.Get());
    CHECK_FALSE(primary->AddFallbackFont(fallback));
    CHECK_FALSE(primary->AddFallbackFont(unloaded));
    CHECK_FALSE(fallback->AddFallbackFont(primary));
    CHECK_FALSE(primary->ResolveGlyph(U'\u0416'));

    primary->ClearFallbackFonts();
    CHECK(primary->GetFallbackFonts().empty());
    CHECK(primary->GetFallbackFontIds().empty());
}

TEST_CASE("Font fallback references discard empty and duplicate asset IDs", "[Renderer][Font][Fallback]")
{
    const UUID first(1, 2, 3, 4);
    const UUID second(5, 6, 7, 8);
    Font font;

    font.SetFallbackFontIds({ UUID::EMPTY, first, first, second });

    REQUIRE(font.GetFallbackFontIds().size() == 2);
    CHECK(font.GetFallbackFontIds()[0] == first);
    CHECK(font.GetFallbackFontIds()[1] == second);
    CHECK(font.GetFallbackFonts().empty());

    font.SetFallbackFontIds({ UUID(1, 0, 0, 0), UUID(2, 0, 0, 0), UUID(3, 0, 0, 0), UUID(4, 0, 0, 0), UUID(5, 0, 0, 0), UUID(6, 0, 0, 0),
                              UUID(7, 0, 0, 0), UUID(8, 0, 0, 0), UUID(9, 0, 0, 0) });
    CHECK(font.GetFallbackFontIds().size() == Font::MAX_FALLBACK_FONTS);
}

TEST_CASE("Font manager provides exact lookup and an optional default fallback", "[Renderer][Font][Manager]")
{
    AssetManager assetManager;
    const AssetHandle<Font> body = CreateFontHandle(assetManager);
    const AssetHandle<Font> replacement = CreateFontHandle(assetManager);

    FontManager::Clear();
    CHECK_FALSE(FontManager::Find("missing"));
    CHECK_FALSE(FontManager::Get("missing"));
    CHECK_FALSE(FontManager::Register("", body));
    CHECK_FALSE(FontManager::Register("body", {}));

    REQUIRE(FontManager::Register("body", body));
    CHECK(FontManager::Contains("body"));
    CHECK(FontManager::Find("body").Get() == body.Get());
    CHECK(FontManager::Get("body").Get() == body.Get());

    FontManager::SetDefaultFont(replacement);
    CHECK(FontManager::Get("missing").Get() == replacement.Get());
    CHECK_FALSE(FontManager::Find("missing"));

    REQUIRE(FontManager::Remove("body"));
    CHECK_FALSE(FontManager::Contains("body"));
    CHECK_FALSE(FontManager::Remove("body"));

    FontManager::Clear();
    CHECK_FALSE(FontManager::GetDefaultFont());
}

TEST_CASE("Font importer validates extensions and font signatures", "[Assets][Importer][Font]")
{
    FontImporter importer;

    CHECK(importer.IsExtensionSupported("ttf"));
    CHECK(importer.IsExtensionSupported(".OTF"));
    CHECK(importer.IsExtensionSupported("TTC"));
    CHECK_FALSE(importer.IsExtensionSupported("woff2"));

    uint8_t trueType[] = { 0x00, 0x01, 0x00, 0x00 };
    uint8_t openType[] = { 'O', 'T', 'T', 'O' };
    uint8_t collection[] = { 't', 't', 'c', 'f' };
    uint8_t legacyTrueType[] = { 't', 'r', 'u', 'e' };
    uint8_t invalid[] = { 'n', 'o', 'p', 'e' };

    CHECK(importer.IsMagicNumSupported(trueType, sizeof(trueType)));
    CHECK(importer.IsMagicNumSupported(openType, sizeof(openType)));
    CHECK(importer.IsMagicNumSupported(collection, sizeof(collection)));
    CHECK(importer.IsMagicNumSupported(legacyTrueType, sizeof(legacyTrueType)));
    CHECK_FALSE(importer.IsMagicNumSupported(invalid, sizeof(invalid)));
    CHECK_FALSE(importer.IsMagicNumSupported(nullptr, 0));
    CHECK_FALSE(importer.IsMagicNumSupported(trueType, 3));
}

TEST_CASE("Font importer rejects options for another asset type", "[Assets][Importer][Font]")
{
    FontImporter importer;
    CHECK(importer.Import("ignored.ttf", CreateRef<ImportOptions>()) == nullptr);
}
