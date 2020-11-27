using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    
    public class Time
    {
        public extern static float deltaTime { [MethodImpl(MethodImplOptions.InternalCall)] get; }
        public extern static float time { [MethodImpl(MethodImplOptions.InternalCall)] get; }
        public extern static float fixedDeltaTime { [MethodImpl(MethodImplOptions.InternalCall)] get; }
        public extern static float smoothDeltaTime { [MethodImpl(MethodImplOptions.InternalCall)]  get; }
        public extern static float realtimeSinceStartup { [MethodImpl(MethodImplOptions.InternalCall)] get; }
        public extern static float frameCount { [MethodImpl(MethodImplOptions.InternalCall)] get; }
    }
}
