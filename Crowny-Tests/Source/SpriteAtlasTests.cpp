#include "Crowny/Renderer/SpriteAtlas.h"
#include "Crowny/Serialization/SpriteAtlasSerializer.h"
#include <catch2/catch_test_macros.hpp>

using namespace Crowny;

namespace
{
    SpriteAtlasInput AtlasImage(uint32_t identity, const glm::vec4& color, uint32_t width = 4, uint32_t height = 4)
    {
        SpriteAtlasInput input;
        input.SpriteId = UUID(identity, 0, 0, 0);
        input.Pixels = PixelData::Create(width, height, TextureFormat::RGBA8);
        for (uint32_t y = 0; y < height; ++y)
            for (uint32_t x = 0; x < width; ++x)
                input.Pixels->SetColorAt(x, y, color);
        return input;
    }
} // namespace

TEST_CASE("Atlas packing is deterministic and preserves identities and sprite metadata", "[2D][SpriteAtlas]")
{
    SpriteAtlasSettings settings;
    settings.PageSize = 32;
    settings.MipLevels = 3;
    Vector<SpriteAtlasInput> inputs{ AtlasImage(3, { 1, 0, 0, 1 }), AtlasImage(1, { 0, 1, 0, 1 }), AtlasImage(2, { 0, 0, 1, 1 }) };
    inputs[0].Metadata.Pivot = { 0, 1 };
    inputs[0].Metadata.OriginalSize = { 50, 25 };
    inputs[0].Metadata.Borders = { 1, 2, 1, 2 };
    SpriteAtlasBuild first, second;
    String error;
    REQUIRE(BuildSpriteAtlas(settings, inputs, first, &error));
    std::reverse(inputs.begin(), inputs.end());
    REQUIRE(BuildSpriteAtlas(settings, inputs, second, &error));
    REQUIRE(first.Entries.size() == 3);
    REQUIRE(first.Pages.size() == second.Pages.size());
    for (size_t i = 0; i < first.Entries.size(); ++i)
    {
        CHECK(first.Entries[i].SpriteId == UUID(static_cast<uint32_t>(i + 1), 0, 0, 0));
        CHECK(first.Entries[i].Geometry.UvRect == second.Entries[i].Geometry.UvRect);
        CHECK(first.Entries[i].Page == second.Entries[i].Page);
    }
    CHECK(first.Entries[2].Geometry.Pivot == glm::vec2(0, 1));
    CHECK(first.Entries[2].Geometry.Size == glm::vec2(0.5f, 0.25f));
    CHECK(first.Entries[2].Metadata.Borders == glm::vec4(1, 2, 1, 2));
    for (size_t page = 0; page < first.Pages.size(); ++page)
        for (size_t mip = 0; mip < first.Pages[page].Mips.size(); ++mip)
        {
            const auto& a = *first.Pages[page].Mips[mip];
            const auto& b = *second.Pages[page].Mips[mip];
            CHECK(std::memcmp(a.GetData(), b.GetData(), a.GetSize()) == 0);
        }
}

TEST_CASE("Atlas pages isolate colors and extrude every configured mip", "[2D][SpriteAtlas]")
{
    SpriteAtlasSettings settings;
    settings.PageSize = 32;
    settings.MipLevels = 3;
    Vector<SpriteAtlasInput> inputs{ AtlasImage(1, { 1, 0, 0, 1 }, 5, 7), AtlasImage(2, { 0, 1, 0, 1 }, 7, 5) };
    SpriteAtlasBuild output;
    REQUIRE(BuildSpriteAtlas(settings, inputs, output));
    REQUIRE(output.Pages.size() == 1);
    for (size_t i = 0; i < output.Entries.size(); ++i)
    {
        const auto& entry = output.Entries[i];
        for (uint32_t mip = 0; mip < settings.MipLevels; ++mip)
        {
            const uint32_t x = static_cast<uint32_t>(entry.Geometry.UvRect.x * settings.PageSize) >> mip;
            const uint32_t y = static_cast<uint32_t>(entry.Geometry.UvRect.y * settings.PageSize) >> mip;
            const auto expected = i == 0 ? glm::vec4(1, 0, 0, 1) : glm::vec4(0, 1, 0, 1);
            CHECK(output.Pages[entry.Page].Mips[mip]->GetColorAt(x, y) == expected);
            CHECK(output.Pages[entry.Page].Mips[mip]->GetColorAt(x - 1, y - 1) == expected);
        }
    }
    inputs[1].SRGB = false;
    REQUIRE(BuildSpriteAtlas(settings, inputs, output));
    REQUIRE(output.Pages.size() == 2);
    CHECK(output.Entries[0].Page != output.Entries[1].Page);
    settings.PageSize = 16;
    inputs[1].SRGB = true;
    REQUIRE(BuildSpriteAtlas(settings, inputs, output));
    REQUIRE(output.Pages.size() == 2);
}

TEST_CASE("Atlas failures preserve the previous result and report a reason", "[2D][SpriteAtlas]")
{
    SpriteAtlasSettings settings;
    settings.PageSize = 16;
    settings.MipLevels = 2;
    Vector<SpriteAtlasInput> inputs{ AtlasImage(1, { 1, 1, 1, 1 }) };
    SpriteAtlasBuild output;
    REQUIRE(BuildSpriteAtlas(settings, inputs, output));
    const auto previous = output.Pages[0].Mips[0];
    SECTION("Oversized image") { inputs[0] = AtlasImage(2, { 1, 0, 0, 1 }, 16, 16); }
    SECTION("Duplicate identity") { inputs.push_back(inputs[0]); }
    SECTION("Memory pressure") { settings.MaxBytes = 1; }
    SECTION("Invalid metadata") { inputs[0].Metadata.PixelsPerUnit = 0; }
    SECTION("Invalid page size") { settings.PageSize = 15; }
    SECTION("Too many mip levels") { settings.MipLevels = 10; }
    String error;
    CHECK_FALSE(BuildSpriteAtlas(settings, inputs, output, &error));
    CHECK_FALSE(error.empty());
    REQUIRE(output.Pages.size() == 1);
    CHECK(output.Pages[0].Mips[0] == previous);
}

TEST_CASE("Atlas source parsing rejects malformed settings transactionally", "[2D][SpriteAtlas][Serialization]")
{
    const auto atlas = CreateRef<SpriteAtlas>();
    atlas->SetName("Original");
    const auto before = atlas->GetData();
    SpriteAtlasSerializer serializer(atlas);
    CHECK_FALSE(serializer.DeserializeFromString("Version: 1\nName: Changed\nPageSize: wrong\n"));
    CHECK_FALSE(serializer.DeserializeFromString("Version: 1\nMipLevels: 0\n"));
    CHECK_FALSE(serializer.DeserializeFromString("Version: 1\nSprites: wrong\n"));
    CHECK(atlas->GetData() == before);
    CHECK(atlas->GetName() == "Original");
    const auto restored = CreateRef<SpriteAtlas>();
    REQUIRE(SpriteAtlasSerializer(restored).DeserializeFromString(serializer.SerializeToString()));
    CHECK(restored->GetData() == before);
    CHECK(restored->GetName() == "Original");
}
