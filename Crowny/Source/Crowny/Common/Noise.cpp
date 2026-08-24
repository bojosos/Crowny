#include "cwpch.h"

#include "Crowny/Common/Noise.h"

#include <FastNoiseLite.h>

namespace Crowny
{
    namespace
    {
        FastNoiseLite::NoiseType ToFastNoiseType(NoiseFunc function)
        {
            switch (function)
            {
            case NoiseFunc::Perlin:
                return FastNoiseLite::NoiseType_Perlin;
            case NoiseFunc::OpenSimplex2:
                return FastNoiseLite::NoiseType_OpenSimplex2;
            case NoiseFunc::Cellular:
                return FastNoiseLite::NoiseType_Cellular;
            case NoiseFunc::OpenSimplex2S:
                return FastNoiseLite::NoiseType_OpenSimplex2S;
            case NoiseFunc::ValueCubic:
                return FastNoiseLite::NoiseType_ValueCubic;
            case NoiseFunc::Value:
                return FastNoiseLite::NoiseType_Value;
            default:
                return FastNoiseLite::NoiseType_OpenSimplex2;
            }
        }

        FastNoiseLite CreateGenerator(const NoiseOptions& options)
        {
            FastNoiseLite generator(options.Seed);
            generator.SetFrequency(options.Smoothness > 0.0f ? 1.0f / options.Smoothness : 0.0f);
            generator.SetNoiseType(ToFastNoiseType(options.NoiseFunc));
            generator.SetRotationType3D(static_cast<FastNoiseLite::RotationType3D>(options.Rotation3D));
            generator.SetFractalType(options.Octaves > 1 ? static_cast<FastNoiseLite::FractalType>(options.Fractal)
                                                         : FastNoiseLite::FractalType_None);
            generator.SetFractalOctaves(std::max(options.Octaves, 1));
            generator.SetFractalLacunarity(options.Lacunarity);
            generator.SetFractalGain(options.Roughness);
            generator.SetFractalWeightedStrength(glm::clamp(options.WeightedStrength, 0.0f, 1.0f));
            generator.SetFractalPingPongStrength(options.PingPongStrength);
            generator.SetCellularDistanceFunction(static_cast<FastNoiseLite::CellularDistanceFunction>(options.CellularDistance));
            generator.SetCellularReturnType(static_cast<FastNoiseLite::CellularReturnType>(options.CellularReturn));
            generator.SetCellularJitter(options.CellularJitter);
            generator.SetDomainWarpType(static_cast<FastNoiseLite::DomainWarpType>(options.DomainWarp));
            generator.SetDomainWarpAmp(options.DomainWarpAmplitude);
            return generator;
        }

        float Normalize(float value) { return glm::clamp((value + 1.0f) * 0.5f, 0.0f, 1.0f); }
    } // namespace

    float Noise::Round(const glm::vec2& coords)
    {
        const auto bump = [](float t) { return glm::max(0.0f, 1.0f - std::pow(t, 6.0f)); };
        const float b = bump(coords.x) * bump(coords.y);
        return b * 0.9f;
    }

    float Noise::Noise2D(const NoiseOptions& ops, float xPos, float yPos) { return Normalize(CreateGenerator(ops).GetNoise(xPos, yPos)); }

    float Noise::Noise2D(const NoiseOptions& ops, const glm::vec2& position) { return Noise2D(ops, position.x, position.y); }

    float Noise::Noise3D(const NoiseOptions& ops, const glm::vec3& position)
    {
        return Normalize(CreateGenerator(ops).GetNoise(position.x, position.y, position.z));
    }

    glm::vec2 Noise::Warp2D(const NoiseOptions& ops, const glm::vec2& position)
    {
        glm::vec2 result = position;
        CreateGenerator(ops).DomainWarp(result.x, result.y);
        return result;
    }

    glm::vec3 Noise::Warp3D(const NoiseOptions& ops, const glm::vec3& position)
    {
        glm::vec3 result = position;
        CreateGenerator(ops).DomainWarp(result.x, result.y, result.z);
        return result;
    }
} // namespace Crowny
