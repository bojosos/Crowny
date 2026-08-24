#include <catch2/catch_test_macros.hpp>

#include "Crowny/Renderer/Gpu2DBatch.h"

using namespace Crowny;

namespace
{
    Gpu2DDrawItem Item(uint32_t objectID, uint32_t pipeline, int32_t layer, int32_t order)
    {
        Gpu2DDrawItem item;
        item.Data.Metadata.y = objectID;
        item.Batch.Pipeline = pipeline;
        item.SortingLayer = layer;
        item.OrderInLayer = order;
        item.StableOrder = objectID;
        return item;
    }
}

TEST_CASE("2D stable batches preserve exact ordering and only merge adjacent items", "[Renderer][2D][Batching]")
{
    const std::array items{ Item(3, 1, 0, 2), Item(1, 1, 0, 0), Item(2, 2, 0, 1), Item(4, 1, 1, 0) };
    Gpu2DBatchBuilder builder;
    Gpu2DDrawList output;
    builder.Build(items.data(), static_cast<uint32_t>(items.size()), Gpu2DOrderingMode::StableLayers, output);

    REQUIRE(output.Instances.size() == 4);
    CHECK(output.Instances[0].Metadata.y == 1);
    CHECK(output.Instances[1].Metadata.y == 2);
    CHECK(output.Instances[2].Metadata.y == 3);
    CHECK(output.Instances[3].Metadata.y == 4);
    REQUIRE(output.Runs.size() == 4);
    CHECK(output.Runs[0].FirstInstance == 0);
    CHECK(output.Runs[3].FirstInstance == 3);
}

TEST_CASE("2D batch-optimized ordering groups weighted particle pipelines within a layer", "[Renderer][2D][Batching]")
{
    auto first = Item(1, 2, 0, 0);
    auto second = Item(2, 1, 0, 1);
    auto third = Item(3, 2, 0, 2);
    first.Batch.Alpha = second.Batch.Alpha = third.Batch.Alpha = AlphaMode::WeightedOIT;
    first.Batch.Primitive = second.Batch.Primitive = third.Batch.Primitive = Gpu2DPrimitive::ParticleSprite;
    const std::array items{ first, second, third };

    Gpu2DBatchBuilder builder;
    Gpu2DDrawList output;
    builder.Build(items.data(), static_cast<uint32_t>(items.size()), Gpu2DOrderingMode::BatchOptimized, output);

    REQUIRE(output.Runs.size() == 2);
    CHECK(output.Runs[0].Batch.Pipeline == 1);
    CHECK(output.Runs[0].InstanceCount == 1);
    CHECK(output.Runs[1].Batch.Pipeline == 2);
    CHECK(output.Runs[1].InstanceCount == 2);
}
