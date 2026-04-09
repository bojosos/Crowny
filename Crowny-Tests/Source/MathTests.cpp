#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Crowny/Common/Math.h"

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
        glm::vec3 forward = Math::GetForwardDirection({0.0f, 0.0f, 0.0f});
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

TEST_CASE("Transform::SpaceConversion", "[Math]")
{
    Transform parent({10.0f, 0.0f, 0.0f}, glm::quat(1, 0, 0, 0), {1.0f, 1.0f, 1.0f});
    Transform child({5.0f, 0.0f, 0.0f}, glm::quat(1, 0, 0, 0), {1.0f, 1.0f, 1.0f});

    SECTION("MakeWorld")
    {
        child.MakeWorld(parent);
        CHECK(child.GetPosition() == glm::vec3(15.0f, 0.0f, 0.0f));
    }

    SECTION("MakeLocal")
    {
        Transform worldChild({15.0f, 0.0f, 0.0f}, glm::quat(1, 0, 0, 0), {1.0f, 1.0f, 1.0f});
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
        CHECK(Math::DivideAndRoundUp(10, 3) == 4);  // ceil(10/3) = 4
        CHECK(Math::DivideAndRoundUp(7, 2) == 4);   // ceil(7/2) = 4
        CHECK(Math::DivideAndRoundUp(1, 2) == 1);   // ceil(1/2) = 1
        CHECK(Math::DivideAndRoundUp(5, 3) == 2);   // ceil(5/3) = 2
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
