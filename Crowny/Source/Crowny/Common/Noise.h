#pragma once

#include <glm/glm.hpp>

namespace Crowny
{

    enum class NoiseFunc
    {
        Perlin,
        Simplex,
        Voronoi
    };

    struct NoiseOptions
    {
        int Octaves;
        float Smoothness;
        float Roughness;
        int Seed;
        NoiseFunc NoiseFunc;
    };

    class Noise
    {
    public:
        static float Round(const glm::vec2& coords);
        static float Noise2D(const NoiseOptions& ops, float xPos, float yPos);
        static float Noise2D(const NoiseOptions& ops, const glm::vec2& position);
        static float Noise3D(const NoiseOptions& ops, const glm::vec3& position);
    };
} // namespace Crowny
