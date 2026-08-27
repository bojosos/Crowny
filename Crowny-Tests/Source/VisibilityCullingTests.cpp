#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Crowny/Renderer/VisibilityCulling.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

using namespace Crowny;

TEST_CASE("Visibility frustum rejects spheres outside any plane", "[Renderer][Visibility]")
{
    VisibilityFrustum frustum;
    frustum.Planes = { glm::vec4(1, 0, 0, 1), glm::vec4(-1, 0, 0, 1), glm::vec4(0, 1, 0, 1),
                       glm::vec4(0, -1, 0, 1), glm::vec4(0, 0, 1, 1), glm::vec4(0, 0, -1, 1) };
    CHECK(frustum.IntersectsSphere(glm::vec3(0.0f), 0.1f));
    CHECK(frustum.IntersectsSphere(glm::vec3(1.05f, 0.0f, 0.0f), 0.1f));
    CHECK_FALSE(frustum.IntersectsSphere(glm::vec3(1.2f, 0.0f, 0.0f), 0.1f));
}

TEST_CASE("Transformed sphere bounds remain conservative under shear", "[Renderer][Visibility]")
{
    const SphereBounds bounds(glm::vec3(1.0f, 2.0f, 3.0f), 1.0f);
    glm::mat4 transform(1.0f);
    transform[1][0] = 1.0f;
    transform[3] = glm::vec4(4.0f, 5.0f, 6.0f, 1.0f);

    const glm::vec4 transformed = VisibilityCulling::TransformSphere(bounds, transform);
    CHECK(glm::vec3(transformed) == glm::vec3(7.0f, 7.0f, 9.0f));
    CHECK(transformed.w == Catch::Approx(std::sqrt(3.0f)));
    CHECK(transformed.w > std::sqrt(2.0f));

    const SphereBounds unit(glm::vec3(0.0f), 2.0f);
    const glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), 0.7f, glm::normalize(glm::vec3(1.0f, 2.0f, 3.0f)));
    CHECK(VisibilityCulling::TransformSphere(unit, rotation).w == Catch::Approx(2.0f).margin(0.0001f));
}

TEST_CASE("Projected size and LOD error scale with distance", "[Renderer][Visibility]")
{
    CHECK(VisibilityCulling::ProjectedSphereDiameter(1.0f, 10.0f, 1.0f, 1000.0f) == Catch::Approx(100.0f));

    MeshGpuGeometry geometry;
    geometry.Lods.resize(3);
    geometry.Lods[0].Error = 0.0f;
    geometry.Lods[1].Error = 0.01f;
    geometry.Lods[2].Error = 0.1f;
    CHECK(VisibilityCulling::SelectLod(geometry, 2.0f, 1.0f, 1000.0f, 1.0f) == 0);
    CHECK(VisibilityCulling::SelectLod(geometry, 10.0f, 1.0f, 1000.0f, 1.0f) == 1);
    CHECK(VisibilityCulling::SelectLod(geometry, 100.0f, 1.0f, 1000.0f, 1.0f) == 2);
}

TEST_CASE("Meshlet normal cones follow meshoptimizer rejection math", "[Renderer][Visibility]")
{
    CHECK(VisibilityCulling::IsMeshletBackfacing(glm::vec3(0.0f, 0.0f, 10.0f), 0.1f,
                                                  glm::vec3(0.0f, 0.0f, 1.0f), 0.5f, glm::vec3(0.0f)));
    CHECK_FALSE(VisibilityCulling::IsMeshletBackfacing(glm::vec3(0.0f, 0.0f, 10.0f), 0.1f,
                                                        glm::vec3(0.0f, 0.0f, -1.0f), 0.5f, glm::vec3(0.0f)));
    CHECK_FALSE(VisibilityCulling::IsMeshletBackfacing(glm::vec3(0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 1.0f),
                                                        0.5f, glm::vec3(0.0f)));
}

TEST_CASE("Reverse Z Hi-Z keeps the farthest occluder conservatively", "[Renderer][Visibility]")
{
    const float covered[] = { 0.8f, 0.75f, 0.7f, 0.72f };
    const float withBackground[] = { 0.8f, 0.75f, 0.0f, 0.72f };
    CHECK(VisibilityCulling::ReduceReverseZ(covered, 4) == Catch::Approx(0.7f));
    CHECK(VisibilityCulling::ReduceReverseZ(withBackground, 4) == 0.0f);
    CHECK(VisibilityCulling::IsOccludedReverseZ(0.5f, 0.7f));
    CHECK_FALSE(VisibilityCulling::IsOccludedReverseZ(0.75f, 0.7f));
    CHECK_FALSE(VisibilityCulling::IsOccludedReverseZ(0.0f, 0.0f));
}
