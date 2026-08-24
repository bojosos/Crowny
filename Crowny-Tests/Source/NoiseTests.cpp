#include "Crowny/Common/Noise.h"
#include <catch2/catch_test_macros.hpp>

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

    SECTION("All FastNoise Lite algorithms")
    {
        const NoiseFunc functions[] = { NoiseFunc::OpenSimplex2, NoiseFunc::OpenSimplex2S, NoiseFunc::Cellular,
                                        NoiseFunc::Perlin,       NoiseFunc::ValueCubic,    NoiseFunc::Value };

        for (const NoiseFunc function : functions)
        {
            ops.NoiseFunc = function;
            const float value2D = Noise::Noise2D(ops, 12.25f, -8.5f);
            const float value3D = Noise::Noise3D(ops, glm::vec3(12.25f, -8.5f, 3.75f));

            CHECK(value2D >= 0.0f);
            CHECK(value2D <= 1.0f);
            CHECK(value3D >= 0.0f);
            CHECK(value3D <= 1.0f);
        }
    }

    SECTION("Fractal and domain warp options")
    {
        ops.NoiseFunc = NoiseFunc::OpenSimplex2;
        ops.Fractal = NoiseFractal::Ridged;
        const float ridged = Noise::Noise2D(ops, 21.0f, 9.0f);
        ops.Fractal = NoiseFractal::PingPong;
        const float pingPong = Noise::Noise2D(ops, 21.0f, 9.0f);

        CHECK(ridged != pingPong);

        ops.Fractal = NoiseFractal::DomainWarpProgressive;
        ops.DomainWarp = NoiseDomainWarp::OpenSimplex2Reduced;
        ops.DomainWarpAmplitude = 20.0f;
        const glm::vec2 position(4.0f, 7.0f);
        const glm::vec2 warped = Noise::Warp2D(ops, position);

        CHECK(warped != position);
        CHECK(Noise::Warp2D(ops, position) == warped);
    }
}
