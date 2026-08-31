namespace Crowny
{
    
    public class Time
    {
        /// <summary>
        /// The time that has passed since the last frame.
        /// </summary>
        /// <returns>Time since the last simulation frame in seconds.</returns>
        public static float deltaTime => ManagedRuntimeContext.TimeGetDeltaTime();
        
        /// <summary>
        /// The scaled time that has passed since simulation started.
        /// </summary>
        /// <returns>Simulation time in seconds.</returns>
        public static float time => ManagedRuntimeContext.TimeGetTime();
        
        /// <summary>
        /// The configured fixed simulation step.
        /// </summary>
        /// <returns>The fixed step in seconds.</returns>
        public static float fixedDeltaTime => ManagedRuntimeContext.TimeGetFixedDeltaTime();

        /// <summary>
        /// The average scaled delta time over the latest simulation frames.
        /// </summary>
        /// <returns>The smoothed delta time in seconds.</returns>
        public static float smoothDeltaTime => ManagedRuntimeContext.TimeGetSmoothDeltaTime();

        /// <summary>
        /// Unscaled application time since startup.
        /// </summary>
        /// <returns>Realtime in seconds.</returns>
        public static float realtimeSinceStartup => ManagedRuntimeContext.TimeGetRealtimeSinceStartup();

        /// <summary>
        /// The number of frames that have been rendered since the beginning.
        /// </summary>
        /// <returns>The number of frames.</returns>
        public static uint frameCount => ManagedRuntimeContext.TimeGetFrameCount();
    }
}
