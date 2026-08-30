namespace Crowny
{
    /// <summary>Audio source playback state.</summary>
    public enum AudioSourceState
    {
        Playing,
        Paused,
        Stopped
    }

    public class AudioSource : Component
    {
        /// <summary>The source volume in the range [0.0, 1.0].</summary>
        public float volume
        {
            get { return ManagedRuntimeContext.AudioSourceGetVolume(EntityId); }
            set { ManagedRuntimeContext.AudioSourceSetVolume(EntityId, value); }
        }

        /// <summary>The source pitch.</summary>
        public float pitch
        {
            get { return ManagedRuntimeContext.AudioSourceGetPitch(EntityId); }
            set { ManagedRuntimeContext.AudioSourceSetPitch(EntityId, value); }
        }

        /// <summary>The minimum audible distance.</summary>
        public float minDistance
        {
            get { return ManagedRuntimeContext.AudioSourceGetMinDistance(EntityId); }
            set { ManagedRuntimeContext.AudioSourceSetMinDistance(EntityId, value); }
        }

        /// <summary>The maximum audible distance.</summary>
        public float maxDistance
        {
            get { return ManagedRuntimeContext.AudioSourceGetMaxDistance(EntityId); }
            set { ManagedRuntimeContext.AudioSourceSetMaxDistance(EntityId, value); }
        }

        /// <summary>Whether playback loops.</summary>
        public bool loop
        {
            get { return ManagedRuntimeContext.AudioSourceGetLoop(EntityId); }
            set { ManagedRuntimeContext.AudioSourceSetLoop(EntityId, value); }
        }

        /// <summary>Whether the source is muted.</summary>
        public bool muted
        {
            get { return ManagedRuntimeContext.AudioSourceGetMuted(EntityId); }
            set { ManagedRuntimeContext.AudioSourceSetMuted(EntityId, value); }
        }

        /// <summary>Whether the source starts when its entity is enabled.</summary>
        public bool playOnAwake
        {
            get { return ManagedRuntimeContext.AudioSourceGetPlayOnAwake(EntityId); }
            set { ManagedRuntimeContext.AudioSourceSetPlayOnAwake(EntityId, value); }
        }

        /// <summary>The current playback position.</summary>
        public float time
        {
            get { return ManagedRuntimeContext.AudioSourceGetTime(EntityId); }
            set { ManagedRuntimeContext.AudioSourceSetTime(EntityId, value); }
        }

        /// <summary>The clip attached to the source.</summary>
        public AudioClip clip
        {
            get { return ManagedRuntimeContext.CreateAsset<AudioClip>(ManagedRuntimeContext.AudioSourceGetClip(EntityId)); }
            set { ManagedRuntimeContext.AudioSourceSetClip(EntityId, value?.uuid ?? UUID.Empty); }
        }

        /// <summary>The current playback state.</summary>
        public AudioSourceState state => (AudioSourceState)ManagedRuntimeContext.AudioSourceGetState(EntityId);

        public void Play() => ManagedRuntimeContext.AudioSourcePlay(EntityId);
        public void Pause() => ManagedRuntimeContext.AudioSourcePause(EntityId);
        public void Stop() => ManagedRuntimeContext.AudioSourceStop(EntityId);
    }
}
