using System;
using System.Runtime.CompilerServices;
using Crowny.Math;

namespace Crowny
{
    public class Noise
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        extern public static float PerlinNoise(float x, float y);
    }
}