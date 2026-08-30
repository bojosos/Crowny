using System;

namespace Crowny
{

    public class Random
    {
        /// <summary>
        /// Returns a random point inside a unit sphere.
        /// </summary>
        /// <value>A random Vector3 with a length of (0, 1).</value>
        public static Vector3 insideUnitSphere
        {
            get { return ManagedRuntimeContext.RandomGetInsideUnitSphere(); }
        }

        /// <summary>
        /// Returns a random point inside a unit circle.
        /// </summary>
        /// <value>A random Vector2 with a length of (0, 1).</value>
        public static Vector2 insideUnitCircle
        {
            get { return ManagedRuntimeContext.RandomGetInsideUnitCircle(); }
        }

        /// <summary>
        /// Returns a random value in the range [0, 1].
        /// </summary>
        /// <returns>Random value in the range.</returns>
        public static float value => ManagedRuntimeContext.RandomGetValue();

        /// <summary>
        /// Initializes the random number generator.
        /// </summary>
        /// <param name="seed">Seed value.</param>
        public static void InitState(int seed) => ManagedRuntimeContext.RandomInitialize(seed);

        /// <summary>
        /// Returns a random value in the range [min, max).
        /// </summary>
        /// <param name="min">The min value.</param>
        /// <param name="max">The max value.</param>
        /// <returns>A random value in the range.</returns>
        public static float Range(float min, float max) => ManagedRuntimeContext.RandomGetRange(min, max);

    }
}
