using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    
    public class Time
    {
        public static float deltaTime { get { return Internal_GetDeltaTime(); } }
        public static float time { get { return Internal_GetTime(); } }
        public static float fixedDeltaTime { get { return Internal_GetFixedDeltaTime(); } }
        public static float smoothDeltaTime { get { return Internal_GetSmoothDeltaTime(); } }
        public static float realtimeSinceStartup { get { return Internal_RealtimeSinceStartup(); } }
        public static float frameCount { get { return Internal_GetFrameCount(); } }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetTime();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetDeltaTime();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetFixedDeltaTime();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetSmoothDeltaTime();
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_RealtimeSinceStartup();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetFrameCount();
    }
}
