namespace Crowny
{
    
    public class Time
    {
        /// <summary>
        /// The time that has passed since the last frame.
        /// </summary>
        /// <returns>Time since last frame in milliseconds.</returns>
        public static float deltaTime => ManagedRuntimeContext.TimeGetDeltaTime();
        
        /// <summary>
        /// The time that has passed since the beginning.
        /// </summary>
        /// <returns>Time in milliseconds.</returns>
        public static float time => ManagedRuntimeContext.TimeGetTime();
        
        /// <summary>
        /// Fixed delta time is fixed xd
        /// </summary>
        /// <returns></returns>
        public static float fixedDeltaTime => ManagedRuntimeContext.TimeGetFixedDeltaTime();

        /// <summary>
        /// smoothDeltaTime = deltaTime + time / (frameCount + 1);
        /// </summary>
        /// <returns>The smoothDeltaTime if milliseconds.</returns>
        public static float smoothDeltaTime => ManagedRuntimeContext.TimeGetSmoothDeltaTime();

        /// <summary>
        /// The in between frame time since the beginning.
        /// </summary>
        /// <returns>The time in milliseconds.</returns>
        public static float realtimeSinceStartup => ManagedRuntimeContext.TimeGetRealtimeSinceStartup();

        /// <summary>
        /// The number of frames that have been rendered since the beginning.
        /// </summary>
        /// <returns>The number of frames.</returns>
        public static float frameCount => ManagedRuntimeContext.TimeGetFrameCount();
    }
}
