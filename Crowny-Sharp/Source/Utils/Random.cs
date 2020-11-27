using System;
using System.Runtime.CompilerServices;
using Crowny.Math;

namespace Crowny
{
    
    public class Random
    {
        public static Vector3 insideUnitSphere
        { 
            get { Vector3 tmp; Internal_UnitSphere(out tmp); return tmp; }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern static float InitState(int seed);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern static float Range(float min, float max);
        
        public extern static float insideUnitCircle  { [MethodImpl(MethodImplOptions.InternalCall)] get; }

        public extern static float value { [MethodImpl(MethodImplOptions.InternalCall)] get; }

        public extern static float rotation { [MethodImpl(MethodImplOptions.InternalCall)] get; }

        public extern static float state { [MethodImpl(MethodImplOptions.InternalCall)] get; }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern static void Internal_UnitSphere(out Vector3 value);

    }
}
