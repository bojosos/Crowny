using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    
    public class Random
    {
        public static Vector3 insideUnitSphere
        { 
            get { Vector3 tmp; Internal_UnitSphere(out tmp); return tmp; }
        }

        public static Vector2 insideUnitCircle 
        { 
            get { Vector2 tmp; Internal_UnitCircle(out tmp); return tmp; }
        }

        public extern static float value { [MethodImpl(MethodImplOptions.InternalCall)] get; }

        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern static float InitState(int seed);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern static float Range(float min, float max);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern static void Internal_UnitSphere(out Vector3 value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern static void Internal_UnitCircle(out Vector2 value);

    }
}
