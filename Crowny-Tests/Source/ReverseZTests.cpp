#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Crowny/Renderer/ReverseZ.h"

#include <glm/gtc/matrix_transform.hpp>

using namespace Crowny;

namespace
{
    float ProjectDepth(const glm::mat4& projection, float distance)
    {
        const glm::vec4 clip = projection * glm::vec4(0.0f, 0.0f, -distance, 1.0f);
        return clip.z / clip.w;
    }
} // namespace

TEST_CASE("Reverse Z finite projections map near to one and far to zero", "[Renderer][Depth]")
{
    const glm::mat4 projection = ReverseZ::Perspective(glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 1000.0f);
    CHECK(ProjectDepth(projection, 0.1f) == Catch::Approx(1.0f));
    CHECK(ProjectDepth(projection, 1000.0f) == Catch::Approx(0.0f).margin(0.000001f));
    CHECK(ReverseZ::LinearDepth(1.0f, 0.1f, 1000.0f) == Catch::Approx(0.1f));
    CHECK(ReverseZ::LinearDepth(0.0f, 0.1f, 1000.0f) == Catch::Approx(1000.0f));
}

TEST_CASE("Reverse Z infinite projections retain precision without a far clip", "[Renderer][Depth]")
{
    const glm::mat4 projection = ReverseZ::Perspective(glm::radians(60.0f), 1.0f, 0.05f);
    CHECK(ProjectDepth(projection, 0.05f) == Catch::Approx(1.0f));
    CHECK(ProjectDepth(projection, 50000.0f) == Catch::Approx(0.000001f));
    CHECK(ReverseZ::LinearDepth(0.25f, 0.05f) == Catch::Approx(0.2f));
    CHECK(std::isinf(ReverseZ::LinearDepth(0.0f, 0.05f)));
}

TEST_CASE("Scene and guide projections share the device depth convention", "[Renderer][2D][Depth]")
{
    glm::mat4 cameraProjection;
    SECTION("Perspective") { cameraProjection = glm::perspectiveRH_NO(glm::radians(60.0f), 1.5f, 0.1f, 100.0f); }
    SECTION("Orthographic") { cameraProjection = glm::orthoRH_NO(-2.0f, 2.0f, -2.0f, 2.0f, 0.1f, 100.0f); }
    for (bool reverse : { false, true })
    {
        const glm::mat4 projection = ReverseZ::ConvertClipDepth(cameraProjection, reverse);
        CHECK(ProjectDepth(projection, 0.1f) == Catch::Approx(reverse ? 1.0f : 0.0f).margin(0.00001f));
        CHECK(ProjectDepth(projection, 100.0f) == Catch::Approx(reverse ? 0.0f : 1.0f).margin(0.00001f));
        CHECK(projection[0][0] == cameraProjection[0][0]);
        CHECK(projection[1][1] == cameraProjection[1][1]);
    }
}
