#include "Editor/ViewportPicking.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

using namespace Crowny;

TEST_CASE("Viewport picking maps the displayed image to the object ID texture", "[Editor][Viewport][Picking]")
{
    const glm::vec4 imageBounds(100.0f, 50.0f, 500.0f, 250.0f);
    const ViewportTextureExtent textureExtent{ 200u, 100u };

    const auto topLeft = ResolveViewportPickPixel(glm::vec2(100.0f, 50.0f), imageBounds, textureExtent);
    REQUIRE(topLeft.has_value());
    CHECK((*topLeft == ViewportPickPixel{ 0u, 99u }));

    const auto center = ResolveViewportPickPixel(glm::vec2(300.0f, 150.0f), imageBounds, textureExtent);
    REQUIRE(center.has_value());
    CHECK((*center == ViewportPickPixel{ 100u, 49u }));

    const glm::vec2 bottomRight(std::nextafter(imageBounds.z, imageBounds.x), std::nextafter(imageBounds.w, imageBounds.y));
    const auto lastPixel = ResolveViewportPickPixel(bottomRight, imageBounds, textureExtent);
    REQUIRE(lastPixel.has_value());
    CHECK((*lastPixel == ViewportPickPixel{ 199u, 0u }));
}

TEST_CASE("Viewport picking rejects unsafe coordinates before integer conversion", "[Editor][Viewport][Picking]")
{
    const glm::vec4 imageBounds(10.0f, 20.0f, 110.0f, 70.0f);
    const ViewportTextureExtent textureExtent{ 100u, 50u };

    CHECK_FALSE(ResolveViewportPickPixel(glm::vec2(110.0f, 30.0f), imageBounds, textureExtent));
    CHECK_FALSE(ResolveViewportPickPixel(glm::vec2(30.0f, 70.0f), imageBounds, textureExtent));
    CHECK_FALSE(ResolveViewportPickPixel(glm::vec2(9.99f, 30.0f), imageBounds, textureExtent));
    CHECK_FALSE(ResolveViewportPickPixel(glm::vec2(30.0f, 19.99f), imageBounds, textureExtent));
    CHECK_FALSE(ResolveViewportPickPixel(glm::vec2(std::numeric_limits<float>::quiet_NaN(), 30.0f), imageBounds, textureExtent));
    CHECK_FALSE(ResolveViewportPickPixel(glm::vec2(std::numeric_limits<float>::infinity(), 30.0f), imageBounds, textureExtent));
    CHECK_FALSE(ResolveViewportPickPixel(glm::vec2(std::numeric_limits<float>::max(), 30.0f), imageBounds, textureExtent));
    CHECK_FALSE(ResolveViewportPickPixel(glm::vec2(30.0f, 30.0f),
                                         glm::vec4(10.0f, 20.0f, std::numeric_limits<float>::infinity(), 70.0f), textureExtent));
    CHECK_FALSE(ResolveViewportPickPixel(glm::vec2(30.0f, 30.0f), glm::vec4(10.0f, 20.0f, 10.0f, 70.0f), textureExtent));
    CHECK_FALSE(ResolveViewportPickPixel(glm::vec2(30.0f, 30.0f), imageBounds, ViewportTextureExtent{}));
}

TEST_CASE("Viewport texture extents are validated once for resize propagation", "[Editor][Viewport][Picking]")
{
    const auto extent = ResolveViewportTextureExtent(glm::vec2(800.75f, 600.25f));
    REQUIRE(extent.has_value());
    CHECK((*extent == ViewportTextureExtent{ 800u, 600u }));

    const auto subpixelExtent = ResolveViewportTextureExtent(glm::vec2(0.5f, 0.25f));
    REQUIRE(subpixelExtent.has_value());
    CHECK((*subpixelExtent == ViewportTextureExtent{ 1u, 1u }));

    CHECK_FALSE(ResolveViewportTextureExtent(glm::vec2(0.0f, 1.0f)));
    CHECK_FALSE(ResolveViewportTextureExtent(glm::vec2(-1.0f, 1.0f)));
    CHECK_FALSE(ResolveViewportTextureExtent(glm::vec2(std::numeric_limits<float>::infinity(), 1.0f)));
    CHECK_FALSE(ResolveViewportTextureExtent(glm::vec2(std::numeric_limits<float>::max(), 1.0f)));
}
