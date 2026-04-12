#include <catch2/catch_test_macros.hpp>
#include "Crowny/Common/Noise.h"

using namespace Crowny;

TEST_CASE("Noise", "[Common]")
{
    NoiseOptions ops;
    ops.Octaves = 4;
    ops.Smoothness = 10.0f;
    ops.Roughness = 0.5f;
    ops.Seed = 123;
    ops.NoiseFunc = NoiseFunc::Perlin;

    SECTION("Noise2D")
    {
        float val1 = Noise::Noise2D(ops, 0.5f, 0.5f);
        float val2 = Noise::Noise2D(ops, 0.5f, 0.5f);
        float val3 = Noise::Noise2D(ops, 0.6f, 0.5f);

        CHECK(val1 == val2);
        CHECK(val1 != val3);
        CHECK(val1 >= 0.0f);
        CHECK(val1 <= 1.0f);
    }

    SECTION("Noise3D")
    {
        float val1 = Noise::Noise3D(ops, glm::vec3(0.5f, 0.5f, 0.5f));
        float val2 = Noise::Noise3D(ops, glm::vec3(0.5f, 0.5f, 0.5f));
        float val3 = Noise::Noise3D(ops, glm::vec3(0.6f, 0.5f, 0.5f));

        CHECK(val1 == val2);
        CHECK(val1 != val3);
        CHECK(val1 >= 0.0f);
        CHECK(val1 <= 1.0f);
    }

    SECTION("Simplex")
    {
        ops.NoiseFunc = NoiseFunc::Simplex;
        float val1 = Noise::Noise3D(ops, glm::vec3(0.5f, 0.5f, 0.5f));
        CHECK(val1 >= 0.0f);
        CHECK(val1 <= 1.0f);
    }
}
