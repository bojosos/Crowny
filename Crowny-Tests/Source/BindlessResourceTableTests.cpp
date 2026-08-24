#include <catch2/catch_test_macros.hpp>

#include "Crowny/Renderer/BindlessResourceTable.h"

using namespace Crowny;

TEST_CASE("Bindless table resolves missing and replaced resources", "[Renderer][Bindless]")
{
    BindlessResourceTable table(4, 99);
    CHECK(table.Resolve({}) == 99);
    CHECK(table.GetDescriptorIndex({}) == 0);

    const BindlessResourceHandle texture = table.Allocate(1234);
    REQUIRE(texture.IsValid());
    CHECK(table.Resolve(texture) == 1234);
    CHECK(table.GetDescriptorIndex(texture) != 0);
    CHECK(table.Replace(texture, 5678));
    CHECK(table.Resolve(texture) == 5678);

    Vector<BindlessResourceUpdate> updates;
    table.DrainUpdates(updates);
    CHECK(updates.size() == 2);
    table.DrainUpdates(updates);
    CHECK(updates.empty());
}

TEST_CASE("Bindless descriptors are not reused before timeline retirement", "[Renderer][Bindless]")
{
    BindlessResourceTable table(2, 7);
    const BindlessResourceHandle first = table.Allocate(100);
    REQUIRE(first.IsValid());
    REQUIRE(table.Release(first, 12));
    CHECK_FALSE(table.Allocate(200).IsValid());
    CHECK(table.Resolve(first) == 7);

    table.Collect(11);
    CHECK_FALSE(table.Allocate(200).IsValid());
    table.Collect(12);

    const BindlessResourceHandle second = table.Allocate(200);
    REQUIRE(second.IsValid());
    CHECK(second.GetIndex() == first.GetIndex());
    CHECK(second.GetGeneration() != first.GetGeneration());
    CHECK_FALSE(table.Replace(first, 300));
}

TEST_CASE("Bindless table reports exhaustion without overwriting live slots", "[Renderer][Bindless]")
{
    BindlessResourceTable table(3);
    const BindlessResourceHandle first = table.Allocate(1);
    const BindlessResourceHandle second = table.Allocate(2);
    REQUIRE(first.IsValid());
    REQUIRE(second.IsValid());
    CHECK_FALSE(table.Allocate(3).IsValid());
    CHECK(table.GetActiveCount() == 2);
}
