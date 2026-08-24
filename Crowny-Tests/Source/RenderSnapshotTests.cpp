#include <catch2/catch_test_macros.hpp>

#include "Crowny/Renderer/RenderSnapshot.h"

using namespace Crowny;

TEST_CASE("2D render order is stable across renderable types", "[Renderer][2D]")
{
    Vector<Renderable2DOrder> items = {
        { Renderable2DType::Text, 0, 1, 0, 30 },
        { Renderable2DType::Sprite, 0, -1, 20, 40 },
        { Renderable2DType::Sprite, 1, 1, -2, 20 },
        { Renderable2DType::Text, 1, 1, 0, 10 },
    };

    std::stable_sort(items.begin(), items.end(), Renderable2DOrderLess);

    CHECK(items[0].SortingLayer == -1);
    CHECK(items[1].OrderInLayer == -2);
    CHECK(items[2].Type == Renderable2DType::Text);
    CHECK(items[2].StableOrder == 10);
    CHECK(items[3].StableOrder == 30);
}
