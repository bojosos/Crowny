namespace Crowny
{
    /// <summary>Behavior used when playback crosses a clip boundary.</summary>
    public enum AnimationWrapMode
    {
        Clamp = 0,
        Loop = 1,
        PingPong = 2
    }

    /// <summary>Current state of an animation component's player.</summary>
    public enum AnimationPlaybackState
    {
        Stopped = 0,
        Playing = 1,
        Paused = 2
    }

    /// <summary>Controls animation playback and deformation for an entity's mesh.</summary>
    public class AnimationComponent : Component
    {
        /// <summary>Clip evaluated by this component.</summary>
        public AnimationClip clip
        {
            get { return ManagedRuntimeContext.CreateAsset<AnimationClip>(ManagedRuntimeContext.AnimationComponentGetClip(EntityId)); }
            set { ManagedRuntimeContext.AnimationComponentSetClip(EntityId, value != null ? value.uuid : UUID.Empty); }
        }

        /// <summary>Playback rate multiplier. Negative values play in reverse.</summary>
        public float speed
        {
            get { return ManagedRuntimeContext.AnimationComponentGetSpeed(EntityId); }
            set { ManagedRuntimeContext.AnimationComponentSetSpeed(EntityId, value); }
        }

        /// <summary>Behavior used when playback crosses a clip boundary.</summary>
        public AnimationWrapMode wrapMode
        {
            get { return (AnimationWrapMode)ManagedRuntimeContext.AnimationComponentGetWrapMode(EntityId); }
            set { ManagedRuntimeContext.AnimationComponentSetWrapMode(EntityId, (int)value); }
        }

        /// <summary>Whether a new runtime player begins in the playing state.</summary>
        public bool playOnAwake
        {
            get { return ManagedRuntimeContext.AnimationComponentGetPlayOnAwake(EntityId); }
            set { ManagedRuntimeContext.AnimationComponentSetPlayOnAwake(EntityId, value); }
        }

        /// <summary>Whether evaluated root-motion deltas are applied to the entity transform.</summary>
        public bool applyRootMotion
        {
            get { return ManagedRuntimeContext.AnimationComponentGetApplyRootMotion(EntityId); }
            set { ManagedRuntimeContext.AnimationComponentSetApplyRootMotion(EntityId, value); }
        }

        /// <summary>Current raw playback time in seconds.</summary>
        public float time
        {
            get { return ManagedRuntimeContext.AnimationComponentGetTime(EntityId); }
            set { ManagedRuntimeContext.AnimationComponentSetTime(EntityId, value); }
        }

        /// <summary>Raw playback time divided by clip length. Looping values are not clamped.</summary>
        public float normalizedTime
        {
            get { return ManagedRuntimeContext.AnimationComponentGetNormalizedTime(EntityId); }
            set { ManagedRuntimeContext.AnimationComponentSetNormalizedTime(EntityId, value); }
        }

        /// <summary>Current playback state.</summary>
        public AnimationPlaybackState state => (AnimationPlaybackState)ManagedRuntimeContext.AnimationComponentGetState(EntityId);

        /// <summary>Starts the assigned clip from the beginning.</summary>
        public void Play() { ManagedRuntimeContext.AnimationComponentPlay(EntityId); }

        /// <summary>Pauses playback without changing its time.</summary>
        public void Pause() { ManagedRuntimeContext.AnimationComponentPause(EntityId); }

        /// <summary>Stops playback and resets its time.</summary>
        public void Stop() { ManagedRuntimeContext.AnimationComponentStop(EntityId); }
    }
}
