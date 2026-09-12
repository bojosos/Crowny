#include "Crowny/Common/Math.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace Crowny;

TEST_CASE("Math::Utility", "[Math]")
{
    SECTION("Signum")
    {
        CHECK(Math::Signum(5.0f) == 1.0f);
        CHECK(Math::Signum(-5.0f) == -1.0f);
        CHECK(Math::Signum(0.0f) == 0.0f);
    }

    SECTION("Mod")
    {
        CHECK(Math::Mod(5.0f, 3) == 2.0f);
        CHECK(Math::Mod(-1.0f, 3) == 2.0f);
    }

    SECTION("Intbound")
    {
        // Test basic intbound functionality
        CHECK(Math::Intbound(0.5f, 1.0f) == 0.5f);
        CHECK(Math::Intbound(0.5f, -1.0f) == 0.5f);
    }
}

TEST_CASE("Math::Directions", "[Math]")
{
    SECTION("Forward Direction")
    {
        // Yaw 0, Pitch 0 -> -Z direction (in some coordinate systems)
        // Let's check what the implementation does.
        glm::vec3 forward = Math::GetForwardDirection({ 0.0f, 0.0f, 0.0f });
        // Yaw 0 + 90 = 90 deg. cos(90)=0, sin(90)=1 -> x=0, z=1. Pitch 0 -> y=0.
        // Result: {-0, -0, -1}
        CHECK_THAT(forward.x, Catch::Matchers::WithinAbs(0.0f, 0.0001f));
        CHECK_THAT(forward.y, Catch::Matchers::WithinAbs(0.0f, 0.0001f));
        CHECK_THAT(forward.z, Catch::Matchers::WithinRel(-1.0f, 0.001f));
    }
}

TEST_CASE("Math::Matrix", "[Math]")
{
    SECTION("Compose/Decompose Roundtrip")
    {
        glm::vec3 pos(1.0f, 2.0f, 3.0f);
        glm::quat rot = glm::angleAxis(glm::radians(45.0f), glm::vec3(0, 1, 0));
        glm::vec3 scale(1.0f, 1.0f, 1.0f);

        glm::mat4 matrix = Math::ComposeMatrix(pos, rot, scale);

        glm::vec3 dPos, dScale;
        glm::quat dRot;
        bool success = Math::DecomposeMatrix(matrix, dPos, dRot, dScale);

        REQUIRE(success);
        CHECK(dPos == pos);
        CHECK(dScale == scale);

        float dot = glm::dot(dRot, rot);
        CHECK_THAT(std::abs(dot), Catch::Matchers::WithinAbs(1.0f, 0.0001f));
    }
}

TEST_CASE("Composed transforms apply scale then rotation then translation", "[Math][2D]")
{
    const glm::vec3 position(17.0f, -31.0f, 8.0f);
    for (const glm::vec3 axis : { glm::vec3(0, 0, 1), glm::normalize(glm::vec3(1, 2, -3)) })
        for (float angle : { 0.0f, 0.7f, -2.3f })
            for (const glm::vec3 scale : { glm::vec3(1), glm::vec3(-2, 3, .5f), glm::vec3(0, 8, 0) })
            {
                const glm::quat rotation = glm::angleAxis(angle, axis);
                const glm::mat4 matrix = Math::ComposeMatrix(position, rotation, scale);
                for (const glm::vec3 point : { glm::vec3(0), glm::vec3(1, -2, 3) })
                {
                    const glm::vec3 expected = position + rotation * (scale * point);
                    const glm::vec4 actual = matrix * glm::vec4(point, 1);
                    for (uint32_t component = 0; component < 3; ++component)
                        CHECK_THAT(actual[component], Catch::Matchers::WithinAbs(expected[component], .00001f));
                    CHECK(actual.w == 1.0f);
                }
            }
}

TEST_CASE("Matrix decomposition preserves rotations across quaternion trace branches", "[Math]")
{
    const glm::vec3 position(1.0f, 2.0f, 3.0f);
    for (const glm::vec3 axis :
         { glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f)) })
    {
        for (float degrees : { -121.0f, -120.0f, -119.0f, 119.0f, 120.0f, 121.0f, 179.0f, 180.0f, 181.0f, 240.0f, 270.0f })
        {
            for (const glm::vec3 scale : { glm::vec3(2.0f, 3.0f, 4.0f), glm::vec3(-2.0f, 3.0f, 4.0f) })
            {
                CAPTURE(axis.x, axis.y, axis.z, degrees, scale.x);
                const glm::quat rotation = glm::angleAxis(glm::radians(degrees), axis);
                const glm::mat4 matrix = Math::ComposeMatrix(position, rotation, scale);
                glm::vec3 decomposedPosition, decomposedScale;
                glm::quat decomposedRotation;
                REQUIRE(Math::DecomposeMatrix(matrix, decomposedPosition, decomposedRotation, decomposedScale));
                const glm::mat4 recomposed = Math::ComposeMatrix(decomposedPosition, decomposedRotation, decomposedScale);
                for (int column = 0; column < 4; ++column)
                    for (int row = 0; row < 4; ++row)
                        CHECK_THAT(recomposed[column][row], Catch::Matchers::WithinAbs(matrix[column][row], 0.0001f));
            }
        }
    }
}

TEST_CASE("Matrix decomposition removes shear before extracting rotation", "[Math]")
{
    const glm::quat rotation = glm::angleAxis(glm::radians(130.0f), glm::normalize(glm::vec3(1.0f, 2.0f, 3.0f)));
    glm::mat4 shear(1.0f);
    shear[1][0] = 0.3f;
    shear[2][0] = -0.2f;
    shear[2][1] = 0.4f;
    const glm::mat4 matrix = Math::ComposeMatrix(glm::vec3(0.0f), rotation, glm::vec3(1.0f)) * shear;
    glm::vec3 position, scale;
    glm::quat decomposedRotation;
    REQUIRE(Math::DecomposeMatrix(matrix, position, decomposedRotation, scale));
    CHECK_THAT(std::abs(glm::dot(decomposedRotation, rotation)), Catch::Matchers::WithinAbs(1.0f, 0.0001f));
}

TEST_CASE("Transform::SpaceConversion", "[Math]")
{
    Transform parent({ 10.0f, 0.0f, 0.0f }, glm::quat(1, 0, 0, 0), { 1.0f, 1.0f, 1.0f });
    Transform child({ 5.0f, 0.0f, 0.0f }, glm::quat(1, 0, 0, 0), { 1.0f, 1.0f, 1.0f });

    SECTION("MakeWorld")
    {
        child.MakeWorld(parent);
        CHECK(child.GetPosition() == glm::vec3(15.0f, 0.0f, 0.0f));
    }

    SECTION("MakeLocal")
    {
        Transform worldChild({ 15.0f, 0.0f, 0.0f }, glm::quat(1, 0, 0, 0), { 1.0f, 1.0f, 1.0f });
        worldChild.MakeLocal(parent);
        CHECK(worldChild.GetPosition() == glm::vec3(5.0f, 0.0f, 0.0f));
    }
}

TEST_CASE("Math::DivideAndRoundUp", "[Math]")
{
    SECTION("Exact division")
    {
        CHECK(Math::DivideAndRoundUp(10, 5) == 2);
        CHECK(Math::DivideAndRoundUp(8, 4) == 2);
        CHECK(Math::DivideAndRoundUp(100, 10) == 10);
    }

    SECTION("Non-exact division rounds up")
    {
        CHECK(Math::DivideAndRoundUp(10, 3) == 4); // ceil(10/3) = 4
        CHECK(Math::DivideAndRoundUp(7, 2) == 4);  // ceil(7/2) = 4
        CHECK(Math::DivideAndRoundUp(1, 2) == 1);  // ceil(1/2) = 1
        CHECK(Math::DivideAndRoundUp(5, 3) == 2);  // ceil(5/3) = 2
    }

    SECTION("Zero dividend")
    {
        CHECK(Math::DivideAndRoundUp(0, 5) == 0);
        CHECK(Math::DivideAndRoundUp(0, 1) == 0);
    }

    SECTION("Dividend equals divisor")
    {
        CHECK(Math::DivideAndRoundUp(5, 5) == 1);
        CHECK(Math::DivideAndRoundUp(1, 1) == 1);
    }

    SECTION("Divisor of one")
    {
        CHECK(Math::DivideAndRoundUp(7, 1) == 7);
        CHECK(Math::DivideAndRoundUp(0, 1) == 0);
        CHECK(Math::DivideAndRoundUp(1, 1) == 1);
    }

    SECTION("Unsigned types")
    {
        CHECK(Math::DivideAndRoundUp(10u, 3u) == 4u);
        CHECK(Math::DivideAndRoundUp(256u, 64u) == 4u);
        CHECK(Math::DivideAndRoundUp(257u, 64u) == 5u);
    }

    SECTION("Typical texture mip-level calculations")
    {
        // Common use case: computing number of blocks for compressed textures
        CHECK(Math::DivideAndRoundUp(1024, 4) == 256);
        CHECK(Math::DivideAndRoundUp(1025, 4) == 257);
        CHECK(Math::DivideAndRoundUp(1, 4) == 1);
        CHECK(Math::DivideAndRoundUp(3, 4) == 1);
        CHECK(Math::DivideAndRoundUp(4, 4) == 1);
        CHECK(Math::DivideAndRoundUp(5, 4) == 2);
    }
}
