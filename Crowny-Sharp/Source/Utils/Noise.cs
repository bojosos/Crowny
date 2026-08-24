using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    public static class Noise
    {
        /// <summary>
        /// Creates a configurable FastNoise Lite generator.
        /// </summary>
        public static FastNoiseLite CreateGenerator(int seed = 1337)
        {
            return new FastNoiseLite(seed);
        }

        /// <summary>
        /// Samples deterministic 2D Perlin noise.
        /// </summary>
        /// <param name="x">The X coordinate to sample at.</param>
        /// <param name="y">The Y coordinate to sample at.</param>
        /// <returns>A pseudo-random value in the range (0, 1).</returns>
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern static float PerlinNoise(float x, float y);
    }
}
