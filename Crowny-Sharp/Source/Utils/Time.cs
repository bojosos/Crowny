using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    
    public class Time
    {
        /// <summary>
        /// The time that has passed since the last frame.
        /// </summary>
        /// <returns>Time since last frame in milliseconds.</returns>
        public extern static float deltaTime { [MethodImpl(MethodImplOptions.InternalCall)] get; }
        
        /// <summary>
        /// The time that has passed since the beginning.
        /// </summary>
        /// <returns>Time in milliseconds.</returns>
        public extern static float time { [MethodImpl(MethodImplOptions.InternalCall)] get; }
        
        /// <summary>
        /// Fixed delta time is fixed xd
        /// </summary>
        /// <returns></returns>
        public extern static float fixedDeltaTime { [MethodImpl(MethodImplOptions.InternalCall)] get; }

        /// <summary>
        /// smoothDeltaTime = deltaTime + time / (frameCount + 1);
        /// </summary>
        /// <returns>The smoothDeltaTime if milliseconds.</returns>
        public extern static float smoothDeltaTime { [MethodImpl(MethodImplOptions.InternalCall)]  get; }

        /// <summary>
        /// The in between frame time since the beginning.
        /// </summary>
        /// <returns>The time in milliseconds.</returns>
        public extern static float realtimeSinceStartup { [MethodImpl(MethodImplOptions.InternalCall)] get; }

        /// <summary>
        /// The number of frames that have been rendered since the beginning.
        /// </summary>
        /// <returns>The number of frames.</returns>
        public extern static float frameCount { [MethodImpl(MethodImplOptions.InternalCall)] get; }
    }
}
