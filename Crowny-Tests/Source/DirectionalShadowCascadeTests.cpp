#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Crowny/Renderer/DirectionalShadowCascades.h"

#include <glm/ext/matrix_transform.hpp>

using namespace Crowny;
using Catch::Matchers::WithinAbs;

TEST_CASE("Directional shadow split calculation is bounded and monotonic", "[Renderer][Shadows]")
{
    Vector<float> splits;
    DirectionalShadowCascadeBuilder::CalculateSplits(0.1f, 200.0f, 3, 0.65f, splits);

    REQUIRE(splits.size() == 4);
    CHECK_THAT(splits.front(), WithinAbs(0.1f, 0.00001f));
    CHECK_THAT(splits.back(), WithinAbs(200.0f, 0.00001f));
    CHECK(splits[0] < splits[1]);
    CHECK(splits[1] < splits[2]);
    CHECK(splits[2] < splits[3]);
}

TEST_CASE("Directional cascades cover their camera slices", "[Renderer][Shadows]")
{
    DirectionalShadowCascadeSettings settings;
    settings.CascadeCount = 3;
    settings.Resolution = 2048;
    settings.ShadowDistance = 120.0f;
    Vector<DirectionalShadowCascade> cascades;
    DirectionalShadowCascadeBuilder::Build(glm::mat4(1.0f), glm::radians(60.0f), 16.0f / 9.0f, 0.1f,
                                            glm::normalize(glm::vec3(0.5f, -1.0f, 0.25f)), settings, cascades);

    REQUIRE(cascades.size() == 3);
    for (uint32_t index = 0; index < cascades.size(); index++)
    {
        CHECK(cascades[index].NearSplit < cascades[index].FarSplit);
        CHECK(cascades[index].BoundingSphere.w > 0.0f);
        CHECK(cascades[index].TexelWorldSize > 0.0f);
        if (index != 0)
            CHECK_THAT(cascades[index - 1u].FarSplit, WithinAbs(cascades[index].NearSplit, 0.0001f));
    }
}

TEST_CASE("Directional cascade centers snap to shadow texels", "[Renderer][Shadows]")
{
    DirectionalShadowCascadeSettings settings;
    settings.CascadeCount = 1;
    settings.Resolution = 1024;
    settings.ShadowDistance = 50.0f;
    const glm::vec3 lightDirection = glm::normalize(glm::vec3(0.3f, -1.0f, 0.2f));
    Vector<DirectionalShadowCascade> first;
    Vector<DirectionalShadowCascade> moved;
    DirectionalShadowCascadeBuilder::Build(glm::mat4(1.0f), glm::radians(60.0f), 1.6f, 0.1f,
                                            lightDirection, settings, first);
    REQUIRE(first.size() == 1);

    glm::mat4 cameraWorld(1.0f);
    cameraWorld[3].x = first[0].TexelWorldSize * 0.1f;
    DirectionalShadowCascadeBuilder::Build(cameraWorld, glm::radians(60.0f), 1.6f, 0.1f,
                                            lightDirection, settings, moved);

    REQUIRE(moved.size() == 1);
    CHECK_THAT(first[0].BoundingSphere.w, WithinAbs(moved[0].BoundingSphere.w, 0.00001f));
    const glm::vec3 referenceUp = std::abs(lightDirection.y) > 0.95f ? glm::vec3(0.0f, 0.0f, 1.0f)
                                                                     : glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 lightRight = glm::normalize(glm::cross(lightDirection, referenceUp));
    const glm::vec3 lightUp = glm::normalize(glm::cross(lightRight, lightDirection));
    for (const DirectionalShadowCascade* cascade : { &first[0], &moved[0] })
    {
        const float x = glm::dot(glm::vec3(cascade->BoundingSphere), lightRight) / cascade->TexelWorldSize;
        const float y = glm::dot(glm::vec3(cascade->BoundingSphere), lightUp) / cascade->TexelWorldSize;
        CHECK_THAT(x, WithinAbs(std::round(x), 0.0001f));
        CHECK_THAT(y, WithinAbs(std::round(y), 0.0001f));
    }
}
