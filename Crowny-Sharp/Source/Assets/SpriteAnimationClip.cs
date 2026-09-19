namespace Crowny
{
    /// <summary>Playback behavior at sprite animation boundaries.</summary>
    public enum SpriteAnimationMode
    {
        /// <summary>Repeat the frames in order.</summary>
        Loop,
        /// <summary>Stop at the final frame.</summary>
        Once,
        /// <summary>Reverse direction at each end without repeating endpoint frames.</summary>
        PingPong
    }

    /// <summary>An authored sequence of sprite identities and frame durations.</summary>
    public class SpriteAnimationClip : Asset
    {
        /// <summary>Cycle length in seconds, including the return traversal for ping-pong clips.</summary>
        public float Duration => ManagedRuntimeContext.SpriteAnimationClipGetDuration(uuid);
        /// <summary>Number of authored frames.</summary>
        public uint FrameCount => ManagedRuntimeContext.SpriteAnimationClipGetFrameCount(uuid);
        /// <summary>Behavior at a cycle boundary.</summary>
        public SpriteAnimationMode Mode => (SpriteAnimationMode)ManagedRuntimeContext.SpriteAnimationClipGetMode(uuid);
    }
}
