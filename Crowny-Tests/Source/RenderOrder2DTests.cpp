#include <catch2/catch_test_macros.hpp>

#include "Crowny/Renderer/RenderOrder2D.h"

#include <algorithm>
#include <limits>
#include <random>

using namespace Crowny;

TEST_CASE("2D radix order agrees with layer order depth and stable identity", "[Renderer][2D]")
{
    std::mt19937 random(41);
    Vector<Renderable2DOrder> items(100000);
    for (uint32_t i = 0; i < items.size(); ++i)
    {
        auto& item = items[i];
        item.Index = i;
        item.Type = i % 2 ? Renderable2DType::Sprite : Renderable2DType::Text;
        item.SortingLayer = static_cast<int32_t>(random() % 11) - 5;
        item.OrderInLayer = static_cast<int32_t>(random() % 19) - 9;
        item.ViewDepth = static_cast<float>(static_cast<int32_t>(random() % 1000) - 500) / 7.0f;
        item.StableOrder = random();
    }
    auto expected = items;
    std::stable_sort(expected.begin(), expected.end(), Renderable2DOrderLess);
    RenderOrder2D order;
    const auto input = items;
    order.Sort(items);
    CHECK(items == expected);
    CHECK(order.GetSortCount() == 1);
    items = input;
    order.Sort(items);
    CHECK(items == expected);
    CHECK(order.GetSortCount() == 1);
    items = input;
    items[10].SortingLayer = std::numeric_limits<int32_t>::min();
    order.Sort(items);
    CHECK(items.front().Index == 10);
    CHECK(order.GetSortCount() == 2);
}

TEST_CASE("2D ordering is deterministic at signed extremes and invalid depth", "[Renderer][2D]")
{
    Vector<Renderable2DOrder> items(5);
    for (uint32_t i = 0; i < items.size(); ++i)
        items[i].Index = i;
    items[0].ViewDepth = std::numeric_limits<float>::infinity();
    items[1].ViewDepth = -2;
    items[2].ViewDepth = 2;
    items[3].OrderInLayer = std::numeric_limits<int32_t>::max();
    items[4].SortingLayer = std::numeric_limits<int32_t>::min();
    RenderOrder2D order;
    order.Sort(items);
    CHECK(items[0].Index == 4);
    CHECK(items[1].Index == 2);
    CHECK(items[2].Index == 0);
    CHECK(items[3].Index == 1);
    CHECK(items[4].Index == 3);
}
