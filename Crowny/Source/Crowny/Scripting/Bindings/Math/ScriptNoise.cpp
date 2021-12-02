#include "cwpch.h"

#include "Crowny/Common/Noise.h"
#include "Crowny/Scripting/Bindings/Math/ScriptNoise.h"

namespace Crowny
{
    ScriptNoise::ScriptNoise() : ScriptObject() {}

    void ScriptNoise::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("PerlinNoise", (void*)&Internal_PerlinNoise);
    }

    float ScriptNoise::Internal_PerlinNoise(float x, float y)
    {
        NoiseOptions ops = { 1, 1.0f, 1.0f, 123, NoiseFunc::Perlin };
        return Noise::Noise2D(ops, x, y);
    }
} // namespace Crowny
