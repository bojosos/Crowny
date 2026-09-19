namespace Crowny
{
    /// <summary>Animates the entity's sprite during simulation, even while invisible.</summary>
    public class SpriteAnimatorComponent : Component
    {
        /// <summary>The clip to play. Changing it resets playback on the next simulation update.</summary>
        public SpriteAnimationClip Clip
        {
            get => ManagedRuntimeContext.CreateAsset<SpriteAnimationClip>(ManagedRuntimeContext.SpriteAnimatorGetClip(EntityId));
            set => ManagedRuntimeContext.SpriteAnimatorSetClip(EntityId, value != null ? value.uuid : UUID.Empty);
        }
        /// <summary>Playback multiplier; negative values traverse the clip backwards.</summary>
        public float Speed
        {
            get => ManagedRuntimeContext.SpriteAnimatorGetSpeed(EntityId);
            set => ManagedRuntimeContext.SpriteAnimatorSetSpeed(EntityId, value);
        }
        /// <summary>Whether playback starts when simulation begins or a new clip is assigned.</summary>
        public bool PlayOnAwake
        {
            get => ManagedRuntimeContext.SpriteAnimatorGetPlayOnAwake(EntityId);
            set => ManagedRuntimeContext.SpriteAnimatorSetPlayOnAwake(EntityId, value);
        }
        /// <summary>Playback time in seconds. Assigning seeks without emitting completion notifications.</summary>
        public float Time
        {
            get => ManagedRuntimeContext.SpriteAnimatorGetTime(EntityId);
            set => ManagedRuntimeContext.SpriteAnimatorSetTime(EntityId, value);
        }
        /// <summary>Whether playback is advancing.</summary>
        public bool IsPlaying => ManagedRuntimeContext.SpriteAnimatorGetIsPlaying(EntityId);
        /// <summary>Current source frame index, or uint.MaxValue for an unavailable or empty clip.</summary>
        public uint FrameIndex => ManagedRuntimeContext.SpriteAnimatorGetFrameIndex(EntityId);
        /// <summary>Starts or resumes playback at the current time. Seek first to restart a finished clip.</summary>
        public void Play() => ManagedRuntimeContext.SpriteAnimatorPlay(EntityId);
        /// <summary>Pauses while preserving the current time.</summary>
        public void Pause() => ManagedRuntimeContext.SpriteAnimatorPause(EntityId);
        /// <summary>Stops and returns to time zero.</summary>
        public void Stop() => ManagedRuntimeContext.SpriteAnimatorStop(EntityId);
        /// <summary>Returns and clears the number of completed cycles (or once-playback completions) since the last call.</summary>
        public ulong ConsumeCompletions() => ManagedRuntimeContext.SpriteAnimatorConsumeCompletions(EntityId);
    }
}
