#include "Editor/ViewportMaterialHighlight.h"
#include "Editor/ViewportPicking.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>

using namespace Crowny;

TEST_CASE("Material target highlights clip to the viewport and near plane", "[Editor][Material][Highlight]")
{
    const glm::vec4 viewport(100, 50, 900, 450);
    const auto crossing = ProjectMaterialHighlightEdge({ -2, 0, 0, 1 }, { 2, 0, 0, 1 }, viewport);
    REQUIRE(crossing);
    CHECK(glm::distance(crossing->first, glm::vec2(100, 250)) < 0.001f);
    CHECK(glm::distance(crossing->second, glm::vec2(900, 250)) < 0.001f);
    const auto nearPlane = ProjectMaterialHighlightEdge({ -0.5f, 0, -2, 1 }, { 0.5f, 0, 0, 1 }, viewport);
    REQUIRE(nearPlane);
    CHECK(glm::distance(nearPlane->first, glm::vec2(500, 250)) < 0.001f);
    CHECK(glm::distance(nearPlane->second, glm::vec2(700, 250)) < 0.001f);
    CHECK_FALSE(ProjectMaterialHighlightEdge({ 0, 0, -3, 1 }, { 0, 1, -2, 1 }, viewport));
    CHECK_FALSE(ProjectMaterialHighlightEdge({ 0, 0, 0, -1 }, { 0, 0, 0, -2 }, viewport));
    CHECK_FALSE(ProjectMaterialHighlightEdge({ 0, 0, 0, 1 }, { 0, 0, 0, 1 }, { 0, 0, 0, 0 }));
}

TEST_CASE("Mesh drops follow the cursor on the ground plane", "[Editor][Viewport][Drop]")
{
    const glm::vec3 eye(0.0f, 10.0f, 10.0f);
    const glm::vec3 forward = glm::normalize(-eye);
    const glm::mat4 vp = glm::perspective(glm::radians(60.0f), 2.0f, 0.1f, 1000.0f) * glm::lookAt(eye, glm::vec3(0.0f), glm::vec3(0, 1, 0));
    const glm::vec4 bounds(100, 50, 900, 450);
    const auto center = ResolveViewportDropPosition({ 500, 250 }, bounds, vp, eye, forward, glm::length(eye));
    REQUIRE(center);
    CHECK(glm::length(*center) < 0.001f);
    const auto right = ResolveViewportDropPosition({ 700, 300 }, bounds, vp, eye, forward, glm::length(eye));
    REQUIRE(right);
    CHECK(right->x > 0.0f);
    CHECK(std::abs(right->y) < 0.001f);
    const glm::vec4 projected = vp * glm::vec4(*right, 1.0f);
    CHECK(std::abs(projected.x / projected.w - 0.5f) < 0.001f);
    CHECK(std::abs(projected.y / projected.w + 0.25f) < 0.001f);
    CHECK_FALSE(ResolveViewportDropPosition({ 900, 250 }, bounds, vp, eye, forward, 10.0f));
}

TEST_CASE("Mesh drops at the horizon use the camera focus plane", "[Editor][Viewport][Drop]")
{
    const glm::vec3 eye(0, 2, 10);
    const glm::vec3 forward(0, 0, -1);
    const glm::mat4 vp = glm::perspective(glm::radians(60.0f), 2.0f, 0.1f, 1000.0f) * glm::lookAt(eye, eye + forward, glm::vec3(0, 1, 0));
    for (const glm::vec2 cursor : { glm::vec2(500, 250), glm::vec2(500, 100), glm::vec2(700, 250) })
    {
        const auto position = ResolveViewportDropPosition(cursor, { 100, 50, 900, 450 }, vp, eye, forward, 10.0f);
        REQUIRE(position);
        CHECK(std::abs(position->z) < 0.001f);
    }
    CHECK_FALSE(ResolveViewportDropPosition({ 500, 250 }, { 100, 50, 900, 450 }, glm::mat4(0.0f), eye, forward, 10.0f));
}

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
    CHECK_FALSE(
      ResolveViewportPickPixel(glm::vec2(30.0f, 30.0f), glm::vec4(10.0f, 20.0f, std::numeric_limits<float>::infinity(), 70.0f), textureExtent));
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
