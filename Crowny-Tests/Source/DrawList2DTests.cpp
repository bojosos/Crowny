#include <catch2/catch_test_macros.hpp>

#include "Crowny/Renderer/DrawList2D.h"

using namespace Crowny;

TEST_CASE("2D batches preserve mixed primitive and texture order", "[Renderer][2D]")
{
    DrawList2D list(2, 3);
    list.Append({ 9, 11 });
    list.Append({ 8, 12 });
    list.Append({ 7, 11 });
    list.Append({ 6, 11 });
    list.Append({ 5, 20, 0, 0, Primitive2D::Glyph });
    list.Append({ 4, 11 });
    const auto instances = list.GetInstances();
    REQUIRE(instances.size() == 6);
    for (size_t i = 0; i < instances.size(); ++i)
        CHECK(instances[i].Instance == 9 - i);
    CHECK(instances[0].TextureSlot == 0);
    CHECK(instances[1].TextureSlot == 1);
    CHECK(instances[2].TextureSlot == 0);
    const auto batches = list.GetBatches();
    REQUIRE(batches.size() == 4);
    CHECK(batches[0].InstanceCount == 3);
    CHECK(batches[1].Break == BatchBreak2D::InstanceCapacity);
    CHECK(batches[2].Primitive == Primitive2D::Glyph);
    CHECK(batches[3].Primitive == Primitive2D::Sprite);
}

TEST_CASE("2D texture rollover reuses slots without losing instances", "[Renderer][2D]")
{
    DrawList2D list(2);
    list.Append({ 0, 10 });
    list.Append({ 1, 11 });
    list.Append({ 2, 10 });
    list.Append({ 3, 12 });
    list.Append({ 4, 12 });
    REQUIRE(list.GetBatches().size() == 2);
    CHECK(list.GetBatches()[1].Break == BatchBreak2D::TextureCapacity);
    CHECK(list.GetBatches()[1].Textures[0] == 12);
    CHECK(list.GetBatches()[1].InstanceCount == 2);
    CHECK(list.GetInstances()[3].TextureSlot == 0);
    CHECK(list.GetInstances()[4].TextureSlot == 0);
}

TEST_CASE("2D batch state boundaries include material and clipping", "[Renderer][2D]")
{
    DrawList2D list;
    list.Append({ 0, 1, 2, 3 });
    list.Append({ 1, 1, 2, 4 });
    list.Append({ 2, 1, 3, 4 });
    list.Append({ 3, 1, 2, 3 });
    REQUIRE(list.GetBatches().size() == 4);
    for (size_t i = 0; i < list.GetInstances().size(); ++i)
        CHECK(list.GetInstances()[i].Instance == i);
    list.Clear();
    CHECK(list.GetBatches().empty());
    CHECK(list.GetInstances().empty());
    list.Append({ 7, 3 });
    CHECK(list.GetBatches()[0].FirstInstance == 0);
}

TEST_CASE("2D lists split 100000 sprites at bounded batch capacity", "[Renderer][2D]")
{
    DrawList2D list(4, 4096);
    list.Reserve(100000, 25);
    for (uint32_t i = 0; i < 100000; ++i)
        list.Append({ i, i % 4 });
    REQUIRE(list.GetInstances().size() == 100000);
    REQUIRE(list.GetBatches().size() == 25);
    uint32_t next = 0;
    for (const DrawBatch2D& batch : list.GetBatches())
    {
        CHECK(batch.FirstInstance == next);
        CHECK(batch.InstanceCount <= 4096);
        next += batch.InstanceCount;
    }
    CHECK(next == 100000);
}

TEST_CASE("2D storage pages split adjacent draws without reordering", "[Renderer][2D]")
{
    DrawList2D list;
    list.Append({ 3, 7, 0, 0, Primitive2D::Sprite, 0 });
    list.Append({ 0, 7, 0, 0, Primitive2D::Sprite, 1 });
    list.Append({ 1, 7, 0, 0, Primitive2D::Sprite, 1 });
    list.Append({ 2, 7, 0, 0, Primitive2D::Sprite, 0 });
    REQUIRE(list.GetBatches().size() == 3);
    CHECK(list.GetBatches()[1].Break == BatchBreak2D::StoragePage);
    CHECK(list.GetBatches()[1].StoragePage == 1);
    CHECK(list.GetBatches()[1].InstanceCount == 2);
    CHECK(list.GetBatches()[2].StoragePage == 0);
    CHECK(list.GetInstances()[0].Instance == 3);
    CHECK(list.GetInstances()[1].Instance == 0);
    CHECK(list.GetInstances()[2].Instance == 1);
    CHECK(list.GetInstances()[3].Instance == 2);
}
