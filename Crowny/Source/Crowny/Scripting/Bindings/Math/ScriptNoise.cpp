#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Math/ScriptNoise.h"
#include "Crowny/Common/Noise.h"

namespace Crowny
{
    void ScriptNoise::InitRuntimeFunctions()
    {
        CWMonoClass* noiseClass = CWMonoRuntime::GetCrownyAssembly()->GetClass("Crowny", "Noise");
        noiseClass->AddInternalCall("PerlinNoise", (void*)&Internal_PerlinNoise);
    }

    float ScriptNoise::Internal_PerlinNoise(float x, float y)
    {
      NoiseOptions ops = { 1, 1.0f, 1.0f, 123, NoiseFunc::Perlin };
      return Noise::Noise2D(ops, x, y);
    }
}
