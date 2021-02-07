using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    public class Noise
    {
        /// <summary>
        /// Returns a pseudo random value samples on a 2D plane.
        /// </summary>
        /// <param name="x">The X coordinate to sample at.</param>
        /// <param name="y">The Y coordinate to sample at.</param>
        /// <returns>A pseudo-random value in the range (0, 1).</returns>
        [MethodImpl(MethodImplOptions.InternalCall)]
        extern public static float PerlinNoise(float x, float y);
    }
}