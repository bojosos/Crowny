#include <catch2/catch_test_macros.hpp>

#include "Crowny/RenderAPI/RenderAPI.h"

using namespace Crowny;

TEST_CASE("Render statistics count primitives for every topology", "[Renderer][Statistics]")
{
    CHECK(RenderAPI::GetPrimitiveCount(DrawMode::POINT_LIST, 7) == 7);
    CHECK(RenderAPI::GetPrimitiveCount(DrawMode::LINE_LIST, 7) == 3);
    CHECK(RenderAPI::GetPrimitiveCount(DrawMode::LINE_STRIP, 7) == 6);
    CHECK(RenderAPI::GetPrimitiveCount(DrawMode::TRIANGLE_LIST, 8) == 2);
    CHECK(RenderAPI::GetPrimitiveCount(DrawMode::TRIANGLE_STRIP, 8) == 6);
    CHECK(RenderAPI::GetPrimitiveCount(DrawMode::TRIANGLE_FAN, 8) == 6);
}

TEST_CASE("Render statistics do not underflow incomplete strips", "[Renderer][Statistics]")
{
    CHECK(RenderAPI::GetPrimitiveCount(DrawMode::LINE_STRIP, 0) == 0);
    CHECK(RenderAPI::GetPrimitiveCount(DrawMode::LINE_STRIP, 1) == 0);
    CHECK(RenderAPI::GetPrimitiveCount(DrawMode::TRIANGLE_STRIP, 0) == 0);
    CHECK(RenderAPI::GetPrimitiveCount(DrawMode::TRIANGLE_STRIP, 2) == 0);
    CHECK(RenderAPI::GetPrimitiveCount(DrawMode::TRIANGLE_FAN, 2) == 0);
}
