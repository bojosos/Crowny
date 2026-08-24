#pragma once

#include <glm/glm.hpp>

namespace Crowny
{

    enum class NoiseFunc
    {
        Perlin = 0,
        OpenSimplex2 = 1,
        Cellular = 2,
        OpenSimplex2S = 3,
        ValueCubic = 4,
        Value = 5,

        // Backward-compatible names.
        Simplex = OpenSimplex2,
        Voronoi = Cellular
    };

    enum class NoiseRotation3D
    {
        None,
        ImproveXYPlanes,
        ImproveXZPlanes
    };

    enum class NoiseFractal
    {
        None,
        FBm,
        Ridged,
        PingPong,
        DomainWarpProgressive,
        DomainWarpIndependent
    };

    enum class NoiseCellularDistance
    {
        Euclidean,
        EuclideanSquared,
        Manhattan,
        Hybrid
    };

    enum class NoiseCellularReturn
    {
        CellValue,
        Distance,
        Distance2,
        Distance2Add,
        Distance2Subtract,
        Distance2Multiply,
        Distance2Divide
    };

    enum class NoiseDomainWarp
    {
        OpenSimplex2,
        OpenSimplex2Reduced,
        BasicGrid
    };

    struct NoiseOptions
    {
        int Octaves = 3;
        float Smoothness = 100.0f;
        float Roughness = 0.5f;
        int Seed = 1337;
        NoiseFunc NoiseFunc = NoiseFunc::OpenSimplex2;
        float Lacunarity = 2.0f;
        NoiseFractal Fractal = NoiseFractal::FBm;
        float WeightedStrength = 0.0f;
        float PingPongStrength = 2.0f;
        NoiseRotation3D Rotation3D = NoiseRotation3D::None;
        NoiseCellularDistance CellularDistance = NoiseCellularDistance::EuclideanSquared;
        NoiseCellularReturn CellularReturn = NoiseCellularReturn::Distance;
        float CellularJitter = 1.0f;
        NoiseDomainWarp DomainWarp = NoiseDomainWarp::OpenSimplex2;
        float DomainWarpAmplitude = 1.0f;
    };

    class Noise
    {
    public:
        static float Round(const glm::vec2& coords);
        static float Noise2D(const NoiseOptions& ops, float xPos, float yPos);
        static float Noise2D(const NoiseOptions& ops, const glm::vec2& position);
        static float Noise3D(const NoiseOptions& ops, const glm::vec3& position);
        static glm::vec2 Warp2D(const NoiseOptions& ops, const glm::vec2& position);
        static glm::vec3 Warp3D(const NoiseOptions& ops, const glm::vec3& position);
    };
} // namespace Crowny
