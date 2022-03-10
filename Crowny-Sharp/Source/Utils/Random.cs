using System;
using System.Runtime.CompilerServices;

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
            get { Internal_UnitSphere(out Vector3 tmp); return tmp; }
        }

        /// <summary>
        /// Returns a random point inside a unit circle.
        /// </summary>
        /// <value>A random Vector2 with a length of (0, 1).</value>
        public static Vector2 insideUnitCircle 
        { 
            get { Internal_UnitCircle(out Vector2 tmp); return tmp; }
        }

        /// <summary>
        /// Returns a random value in the range [0, 1].
        /// </summary>
        /// <returns>Random value in the range.</returns>
        public extern static float value { [MethodImpl(MethodImplOptions.InternalCall)] get; }

        /// <summary>
        /// Initializes the random number generator.
        /// </summary>
        /// <param name="seed">Seed value.</param>
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern static void InitState(int seed);

        /// <summary>
        /// Returns a random value in the range [min, max).
        /// </summary>
        /// <param name="min">The min value.</param>
        /// <param name="max">The max value.</param>
        /// <returns>A random value in the range.</returns>
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern static float Range(float min, float max);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern static void Internal_UnitSphere(out Vector3 value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern static void Internal_UnitCircle(out Vector2 value);

    }
}
